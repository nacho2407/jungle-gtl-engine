#include "Render/Scene/Scene.h"

#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Engine/Geometry/Frustum.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Particle/ParticleSystemComponent.h"
#include "Render/Scene/DecalCommandBuilder.h"
#include "Render/Scene/ParticleSystemRenderProxy.h"
#include "Render/Scene/PrimitiveDrawCommandBuilder.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCollector.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace
{
bool UsesCameraDependentRenderBounds(const UPrimitiveComponent* PrimitiveComponent)
{
	if (PrimitiveComponent == nullptr)
	{
		return false;
	}

	switch (PrimitiveComponent->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_Billboard:
	case EPrimitiveType::EPT_Text:
	case EPrimitiveType::EPT_SubUV:
		return true;
	default:
		return false;
	}
}

FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
{
	const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
	const UBillboardComponent* Billboard = static_cast<const UBillboardComponent*>(Primitive);
	return UBillboardComponent::MakeBillboardWorldMatrix(
		WorldMatrix.GetOrigin(),
		Billboard->GetBillboardWorldScale(),
		RenderBus.GetCameraForward(),
		RenderBus.GetCameraRight(),
		RenderBus.GetCameraUp());
}

FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
{
	const FVector WorldScale = SubUVComp->GetBillboardWorldScale();
	return UBillboardComponent::MakeBillboardWorldMatrix(
		SubUVComp->GetWorldLocation(),
		FVector(
			WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
			SubUVComp->GetWidth() * WorldScale.Y * 0.5f,
			SubUVComp->GetHeight() * WorldScale.Z * 0.5f),
		RenderBus.GetCameraForward(),
		RenderBus.GetCameraRight(),
		RenderBus.GetCameraUp());
}

FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
{
	static constexpr FVector LocalQuadCorners[4] = {
		FVector(0.0f, -0.5f, 0.5f),
		FVector(0.0f, 0.5f, 0.5f),
		FVector(0.0f, 0.5f, -0.5f),
		FVector(0.0f, -0.5f, -0.5f)
	};

	FAABB Box;
	Box.Reset();

	for (const FVector& Corner : LocalQuadCorners)
	{
		Box.Expand(WorldMatrix.TransformPosition(Corner));
	}

	return Box;
}

FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
{
	switch (PrimitiveComponent->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_Billboard:
		return BuildQuadAABB(MakeViewBillboardMatrix(PrimitiveComponent, RenderBus));
	case EPrimitiveType::EPT_Text:
	{
		const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
		return BuildQuadAABB(TextComp->GetTextMatrix());
	}
	case EPrimitiveType::EPT_SubUV:
	{
		const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
		return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
	}
	default:
		return PrimitiveComponent->GetWorldAABB();
	}
}

FAABB BuildSphereAABB(const FVector& Center, float Radius)
{
	const FVector Extent(Radius, Radius, Radius);
	return FAABB(Center - Extent, Center + Extent);
}

bool BuildLocalShadowLightQuerySphere(const FShadowLightRequest& Request, FVector& OutCenter, float& OutRadius)
{
	OutCenter = Request.WorldLocation;
	OutRadius = 0.0f;

	switch (Request.Type)
	{
	case EShadowLightType::SLT_Point:
	{
		const UPointLightComponent* PointLight = Cast<UPointLightComponent>(Request.LightComponent);
		if (PointLight == nullptr || PointLight->AttenuationRadius <= 0.0f)
		{
			return false;
		}

		OutCenter = PointLight->GetWorldLocation();
		OutRadius = PointLight->AttenuationRadius;
		return true;
	}
	case EShadowLightType::SLT_Spot:
	{
		const USpotlightComponent* SpotLight = Cast<USpotlightComponent>(Request.LightComponent);
		if (SpotLight == nullptr || SpotLight->AttenuationRadius <= 0.0f)
		{
			return false;
		}

		const float SpotAngle = MathUtil::Clamp(
			std::max(SpotLight->OuterConeAngle, SpotLight->InnerConeAngle),
			0.0f,
			89.0f);
		const float Attenuation = SpotLight->AttenuationRadius;

		OutCenter = SpotLight->GetWorldLocation();
		OutRadius = Attenuation;

		if (SpotAngle <= 45.0f)
		{
			const FVector LightDirection = SpotLight->GetForwardVector().GetSafeNormal();
			const float Offset = Attenuation * 0.5f;
			const float BaseRadius = Attenuation * std::tan(MathUtil::DegreesToRadians(SpotAngle));

			if (!LightDirection.IsNearlyZero())
			{
				OutCenter += LightDirection * Offset;
			}
			OutRadius = std::sqrt((Offset * Offset) + (BaseRadius * BaseRadius));
		}

		return OutRadius > 0.0f;
	}
	default:
		return false;
	}
}

bool LocalShadowLightInfluenceIntersectsView(const FShadowLightRequest& Request, const FFrustum& ViewFrustum)
{
	FVector Center(0.0f, 0.0f, 0.0f);
	float Radius = 0.0f;
	if (!BuildLocalShadowLightQuerySphere(Request, Center, Radius))
	{
		return false;
	}

	return ViewFrustum.Intersects(BuildSphereAABB(Center, Radius)) !=
		   FFrustum::EFrustumIntersectResult::Outside;
}

bool HasDirectionalShadowRequest(const FRenderBus& RenderBus)
{
	for (const FShadowLightRequest& Request : RenderBus.ShadowLightRequests)
	{
		if (Request.bCastShadows && Request.Type == EShadowLightType::SLT_Directional)
		{
			return true;
		}
	}

	return false;
}

bool IsShadowRenderablePrimitive(const UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr)
	{
		return false;
	}

	switch (Primitive->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_StaticMesh:
	case EPrimitiveType::EPT_SkeletalMesh:
	case EPrimitiveType::EPT_ProceduralMesh:
		return true;
	default:
		return false;
	}
}

class FCommandBuilderPrimitiveSceneProxy : public FPrimitiveRenderProxy
{
public:
	explicit FCommandBuilderPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
		: Component(InComponent)
	{
	}

protected:
	void CollectSurfaceCommands(const FPrimitiveRenderProxyCollectionContext& Context) const
	{
		if (Component == nullptr)
		{
			return;
		}

		if (Context.Intent == EPrimitiveRenderProxyCollectIntent::ShadowOnly)
		{
			Context.CommandServices.PrimitiveDrawCommandBuilder.CollectShadowCasterPrimitive(
				Component,
				Context.ShowFlags,
				Context.ViewMode,
				Context.RenderBus,
				Context.ResourceProvider);
			return;
		}

		Context.CommandServices.PrimitiveDrawCommandBuilder.CollectPrimitive(
			Component,
			Context.ShowFlags,
			Context.ViewMode,
			Context.RenderBus,
			Context.ResourceProvider);
	}

	UPrimitiveComponent* Component = nullptr;
};

class FStaticMeshSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		CollectSurfaceCommands(Context);
	}
};

class FSkeletalMeshSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		CollectSurfaceCommands(Context);
	}
};

class FProceduralMeshSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		CollectSurfaceCommands(Context);
	}
};

class FBillboardSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		if (Context.Intent == EPrimitiveRenderProxyCollectIntent::ShadowOnly)
		{
			return;
		}

		CollectSurfaceCommands(Context);
	}
};

class FTextSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		if (Context.Intent == EPrimitiveRenderProxyCollectIntent::ShadowOnly)
		{
			return;
		}

		CollectSurfaceCommands(Context);
	}
};

class FSubUVSceneProxy final : public FCommandBuilderPrimitiveSceneProxy
{
public:
	using FCommandBuilderPrimitiveSceneProxy::FCommandBuilderPrimitiveSceneProxy;

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		if (Context.Intent == EPrimitiveRenderProxyCollectIntent::ShadowOnly)
		{
			return;
		}

		CollectSurfaceCommands(Context);
	}
};

class FDecalSceneProxy final : public FPrimitiveRenderProxy
{
public:
	explicit FDecalSceneProxy(UPrimitiveComponent* InComponent)
		: Component(InComponent)
	{
	}

	void CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context) override
	{
		if (Component == nullptr || Context.Intent == EPrimitiveRenderProxyCollectIntent::ShadowOnly)
		{
			return;
		}

		Context.CommandServices.DecalCommandBuilder.CollectDecal(
			Component,
			Context.ShowFlags,
			Context.RenderBus,
			Context.ResourceProvider,
			Context.CommandServices.OBBQueryScratch);
	}

private:
	UPrimitiveComponent* Component = nullptr;
};
} // namespace

void FSceneRenderResources::Initialize(ID3D11Device* InDevice)
{
	if (Device == InDevice)
	{
		return;
	}

	Release();

	if (InDevice == nullptr)
	{
		return;
	}

	Device = InDevice;
	MeshBufferManager.Create(InDevice);
}

void FSceneRenderResources::Release()
{
	if (Device == nullptr)
	{
		return;
	}

	MeshBufferManager.Release();
	Device = nullptr;
}

FMeshBuffer& FSceneRenderResources::GetMeshBuffer(EPrimitiveType InPrimitiveType)
{
	return MeshBufferManager.GetMeshBuffer(InPrimitiveType);
}

FMeshBuffer* FSceneRenderResources::GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel)
{
	return MeshBufferManager.GetStaticMeshBuffer(StaticMeshAsset, LODLevel);
}

FMeshBuffer* FSceneRenderResources::GetProcMeshBuffer(uint32 ProcMeshCompUUID, const TArray<FNormalVertex>& Vertices,
                                                      const TArray<uint32>& Indices)
{
	return MeshBufferManager.GetProcMeshBuffer(ProcMeshCompUUID, Vertices, Indices);
}

FMeshBuffer* FSceneRenderResources::GetCPUSkeletalMeshBuffer(uint32 SkeletalMeshCompUUID, const USkeletalMesh* SkeletalMeshAsset,
                                                            const TArray<FSkeletalMeshVertex>& Vertices,
                                                            const TArray<uint32>& Indices, bool bNeedsUpload)
{
	return MeshBufferManager.GetCPUSkeletalMeshBuffer(
		SkeletalMeshCompUUID,
		SkeletalMeshAsset,
		Vertices,
		Indices,
		bNeedsUpload);
}

FMeshBuffer* FSceneRenderResources::GetGPUSkeletalMeshBuffer(const USkeletalMesh* SkeletalMeshAsset)
{
	return MeshBufferManager.GetGPUSkeletalMeshBuffer(SkeletalMeshAsset);
}

FConstantBuffer* FSceneRenderResources::GetGPUSkeletalBoneMatrixBuffer(uint32 SkeletalMeshCompUUID,
                                                                       const FBoneMatrixConstants& Constants,
                                                                       bool bNeedsUpload)
{
	return MeshBufferManager.GetGPUSkeletalBoneMatrixBuffer(SkeletalMeshCompUUID, Constants, bNeedsUpload);
}

FScene::FScene(UWorld* InWorld)
	: World(InWorld)
{
}

void FScene::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
{
	Device = InDevice;
	DeviceContext = InDeviceContext;
	RenderResources.Initialize(InDevice);
}

void FScene::Release()
{
	Clear();
	RenderResources.Release();
	Device = nullptr;
	DeviceContext = nullptr;
}

void FScene::Clear()
{
	for (FScenePrimitiveEntry& Entry : PrimitiveEntries)
	{
		if (Entry.Proxy)
		{
			Entry.Proxy->ReleaseResources();
		}
		if (Entry.Component != nullptr && Entry.Component->RegisteredScene == this)
		{
			Entry.Component->RegisteredScene = nullptr;
		}
	}

	PrimitiveEntries.clear();
	PrimitiveToEntryIndex.clear();
	LightEntries.clear();
	LightToEntryIndex.clear();
	DirtyPrimitiveCount = 0;
}

void FScene::Rebuild(UWorld* InWorld)
{
	Clear();
	SetWorld(InWorld);

	if (World == nullptr)
	{
		return;
	}

	for (AActor* Actor : World->GetActors())
	{
		RegisterActor(Actor);
	}
}

void FScene::RegisterActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		RegisterPrimitive(Primitive);
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
		{
			RegisterLight(Light);
		}
	}
}

void FScene::UnregisterActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return;
	}

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		UnregisterPrimitive(Primitive);
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (ULightComponentBase* Light = Cast<ULightComponentBase>(Component))
		{
			UnregisterLight(Light);
		}
	}
}

void FScene::RegisterPrimitive(UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr)
	{
		return;
	}

	if (Primitive->RegisteredScene != nullptr && Primitive->RegisteredScene != this)
	{
		Primitive->RegisteredScene->UnregisterPrimitive(Primitive);
	}

	FScenePrimitiveEntry* ExistingEntry = FindPrimitiveEntry(Primitive);
	if (ExistingEntry != nullptr)
	{
		RefreshPrimitiveEntry(*ExistingEntry);
		return;
	}

	FScenePrimitiveEntry Entry;
	Entry.Component = Primitive;
	RefreshPrimitiveEntry(Entry);
	Entry.Proxy = CreatePrimitiveProxy(Primitive);

	const int32 EntryIndex = static_cast<int32>(PrimitiveEntries.size());
	PrimitiveEntries.push_back(std::move(Entry));
	PrimitiveToEntryIndex[Primitive] = EntryIndex;
	Primitive->RegisteredScene = this;
}

void FScene::UnregisterPrimitive(UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr)
	{
		return;
	}

	const auto It = PrimitiveToEntryIndex.find(Primitive);
	if (It == PrimitiveToEntryIndex.end())
	{
		if (Primitive->RegisteredScene == this)
		{
			Primitive->RegisteredScene = nullptr;
		}
		return;
	}

	const int32 EntryIndex = It->second;
	const int32 LastIndex = static_cast<int32>(PrimitiveEntries.size()) - 1;
	if (EntryIndex < 0 || EntryIndex > LastIndex)
	{
		PrimitiveToEntryIndex.erase(It);
		Primitive->RegisteredScene = nullptr;
		return;
	}

	if (PrimitiveEntries[EntryIndex].DirtyFlags != 0u && DirtyPrimitiveCount > 0)
	{
		--DirtyPrimitiveCount;
	}

	if (PrimitiveEntries[EntryIndex].Proxy)
	{
		PrimitiveEntries[EntryIndex].Proxy->ReleaseResources();
	}

	if (EntryIndex != LastIndex)
	{
		PrimitiveEntries[EntryIndex] = std::move(PrimitiveEntries[LastIndex]);
		if (PrimitiveEntries[EntryIndex].Component != nullptr)
		{
			PrimitiveToEntryIndex[PrimitiveEntries[EntryIndex].Component] = EntryIndex;
		}
	}

	PrimitiveEntries.pop_back();
	PrimitiveToEntryIndex.erase(It);

	if (Primitive->RegisteredScene == this)
	{
		Primitive->RegisteredScene = nullptr;
	}
}

void FScene::RegisterLight(ULightComponentBase* Light)
{
	if (Light == nullptr)
	{
		return;
	}

	FSceneLightEntry* ExistingEntry = FindLightEntry(Light);
	if (ExistingEntry != nullptr)
	{
		RefreshLightEntry(*ExistingEntry);
		return;
	}

	FSceneLightEntry Entry;
	Entry.Component = Light;
	RefreshLightEntry(Entry);

	const int32 EntryIndex = static_cast<int32>(LightEntries.size());
	LightEntries.push_back(Entry);
	LightToEntryIndex[Light] = EntryIndex;
}

void FScene::UnregisterLight(ULightComponentBase* Light)
{
	if (Light == nullptr)
	{
		return;
	}

	const auto It = LightToEntryIndex.find(Light);
	if (It == LightToEntryIndex.end())
	{
		return;
	}

	const int32 EntryIndex = It->second;
	const int32 LastIndex = static_cast<int32>(LightEntries.size()) - 1;
	if (EntryIndex < 0 || EntryIndex > LastIndex)
	{
		LightToEntryIndex.erase(It);
		return;
	}

	if (EntryIndex != LastIndex)
	{
		LightEntries[EntryIndex] = LightEntries[LastIndex];
		if (LightEntries[EntryIndex].Component != nullptr)
		{
			LightToEntryIndex[LightEntries[EntryIndex].Component] = EntryIndex;
		}
	}

	LightEntries.pop_back();
	LightToEntryIndex.erase(It);
}

void FScene::MarkPrimitiveDirty(UPrimitiveComponent* Primitive, ESceneProxyDirtyFlag DirtyFlag)
{
	if (Primitive == nullptr)
	{
		return;
	}

	FScenePrimitiveEntry* Entry = FindPrimitiveEntry(Primitive);
	if (Entry == nullptr)
	{
		return;
	}

	if (Entry->DirtyFlags == 0u)
	{
		++DirtyPrimitiveCount;
	}

	Entry->DirtyFlags |= ToSceneProxyDirtyMask(DirtyFlag);
	RefreshPrimitiveEntry(*Entry);

	if ((ToSceneProxyDirtyMask(DirtyFlag) & ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::ParticleTemplate)) != 0u)
	{
		if (Entry->Proxy)
		{
			Entry->Proxy->ReleaseResources();
		}
		Entry->Proxy = CreatePrimitiveProxy(Primitive);
	}

	if (ShouldMarkSpatialBoundsDirty(DirtyFlag) && World != nullptr)
	{
		World->GetSpatialIndex().MarkPrimitiveDirty(Primitive);
	}
}

FPrimitiveRenderProxy* FScene::GetPrimitiveRenderProxy(UPrimitiveComponent* Primitive)
{
	FScenePrimitiveEntry* Entry = FindPrimitiveEntry(Primitive);
	return Entry != nullptr ? Entry->Proxy.get() : nullptr;
}

const FPrimitiveRenderProxy* FScene::GetPrimitiveRenderProxy(UPrimitiveComponent* Primitive) const
{
	const FScenePrimitiveEntry* Entry = FindPrimitiveEntry(Primitive);
	return Entry != nullptr ? Entry->Proxy.get() : nullptr;
}

void FScene::CollectView(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
                         FRenderBus& RenderBus, const FFrustum* ViewFrustum,
                         bool bIncludeEditorOnlyPrimitives)
{
	Collector.BeginSceneCollection(RenderResources);

	if (World == nullptr)
	{
		return;
	}

	const EWorldType WorldType = World->GetWorldType();
	CollectRegisteredLights(Collector, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);

	if (ViewFrustum != nullptr)
	{
		CollectFrustumPrimitives(
			Collector,
			*ViewFrustum,
			ShowFlags,
			ViewMode,
			RenderBus,
			WorldType,
			bIncludeEditorOnlyPrimitives);
		CollectDirectionalShadowCasters(Collector, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
		CollectLocalShadowCasters(Collector, *ViewFrustum, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
		return;
	}

	CollectFullScanPrimitives(Collector, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
}

void FScene::CollectRegisteredLights(FRenderCollector& Collector, FRenderBus& RenderBus, EWorldType WorldType,
                                     bool bIncludeEditorOnlyPrimitives)
{
	for (FSceneLightEntry& Entry : LightEntries)
	{
		RefreshLightEntry(Entry);
		if (ShouldIncludeLight(Entry, WorldType, bIncludeEditorOnlyPrimitives))
		{
			Collector.CollectSceneLight(Entry.Component, RenderBus);
		}
	}
}

void FScene::CollectFullScanPrimitives(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                       FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	for (FScenePrimitiveEntry& Entry : PrimitiveEntries)
	{
		RefreshPrimitiveEntry(Entry);
		if (!IsPrimitiveVisibleForScene(Entry))
		{
			continue;
		}

		Collector.NoteVisiblePrimitive();
		if (ShouldIncludePrimitive(Entry, WorldType, bIncludeEditorOnlyPrimitives))
		{
			Collector.CollectScenePrimitive(
				Entry.Component,
				ShowFlags,
				ViewMode,
				RenderBus,
				WorldType,
				bIncludeEditorOnlyPrimitives);
		}
	}
}

void FScene::CollectFrustumPrimitives(FRenderCollector& Collector, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
                                      EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType,
                                      bool bIncludeEditorOnlyPrimitives)
{
	VisiblePrimitiveScratch.clear();
	World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

	for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
	{
		FScenePrimitiveEntry* Entry = FindPrimitiveEntry(Primitive);
		if (Entry == nullptr)
		{
			continue;
		}

		RefreshPrimitiveEntry(*Entry);
		if (!IsPrimitiveVisibleForScene(*Entry) ||
			UsesCameraDependentRenderBounds(Primitive) ||
			!Entry->bEnableCull)
		{
			continue;
		}

		Collector.NoteBVHPassedPrimitive();
		if (ShouldIncludePrimitive(*Entry, WorldType, bIncludeEditorOnlyPrimitives))
		{
			Collector.CollectScenePrimitive(
				Primitive,
				ShowFlags,
				ViewMode,
				RenderBus,
				WorldType,
				bIncludeEditorOnlyPrimitives);
		}
	}

	std::unordered_set<UPrimitiveComponent*> CollectedFallbackPrimitives;
	CollectedFallbackPrimitives.reserve(32);

	for (FScenePrimitiveEntry& Entry : PrimitiveEntries)
	{
		RefreshPrimitiveEntry(Entry);
		if (!IsPrimitiveVisibleForScene(Entry))
		{
			continue;
		}

		Collector.NoteVisiblePrimitive();

		UPrimitiveComponent* Primitive = Entry.Component;
		const bool bIsCameraDependent = UsesCameraDependentRenderBounds(Primitive);
		const bool bIsUncullable = !Entry.bEnableCull;

		if (!bIsCameraDependent && !bIsUncullable)
		{
			continue;
		}

		if (!CollectedFallbackPrimitives.insert(Primitive).second)
		{
			continue;
		}

		if (bIsCameraDependent && !bIsUncullable &&
			ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) == FFrustum::EFrustumIntersectResult::Outside)
		{
			continue;
		}

		Collector.NoteFallbackPassedPrimitive();
		if (ShouldIncludePrimitive(Entry, WorldType, bIncludeEditorOnlyPrimitives))
		{
			Collector.CollectScenePrimitive(
				Primitive,
				ShowFlags,
				ViewMode,
				RenderBus,
				WorldType,
				bIncludeEditorOnlyPrimitives);
		}
	}
}

void FScene::CollectDirectionalShadowCasters(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                             FRenderBus& RenderBus, EWorldType WorldType,
                                             bool bIncludeEditorOnlyPrimitives)
{
	if (RenderBus.ShadowLightRequests.empty() || !HasDirectionalShadowRequest(RenderBus))
	{
		return;
	}

	std::unordered_set<UPrimitiveComponent*> CollectedPrimitives;
	const TArray<FRenderCommand>& ShadowCommands = RenderBus.GetShadowCasterCommands();
	CollectedPrimitives.reserve(ShadowCommands.size() + 64);

	for (const FRenderCommand& Command : ShadowCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}

	for (FScenePrimitiveEntry& Entry : PrimitiveEntries)
	{
		RefreshPrimitiveEntry(Entry);
		if (!ShouldIncludePrimitive(Entry, WorldType, bIncludeEditorOnlyPrimitives) ||
			!IsShadowRenderablePrimitive(Entry.Component))
		{
			continue;
		}

		if (!CollectedPrimitives.insert(Entry.Component).second)
		{
			continue;
		}

		Collector.CollectSceneShadowCaster(
			Entry.Component,
			ShowFlags,
			ViewMode,
			RenderBus,
			WorldType,
			bIncludeEditorOnlyPrimitives);
	}
}

void FScene::CollectLocalShadowCasters(FRenderCollector& Collector, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
                                       EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType,
                                       bool bIncludeEditorOnlyPrimitives)
{
	if (RenderBus.ShadowLightRequests.empty())
	{
		return;
	}

	std::unordered_set<UPrimitiveComponent*> CollectedPrimitives;
	const TArray<FRenderCommand>& ShadowCommands = RenderBus.GetShadowCasterCommands();
	CollectedPrimitives.reserve(ShadowCommands.size() + 32);

	for (const FRenderCommand& Command : ShadowCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}

	for (const FShadowLightRequest& Request : RenderBus.ShadowLightRequests)
	{
		if (Request.Type != EShadowLightType::SLT_Point && Request.Type != EShadowLightType::SLT_Spot)
		{
			continue;
		}

		if (!LocalShadowLightInfluenceIntersectsView(Request, ViewFrustum))
		{
			continue;
		}

		FVector QueryCenter(0.0f, 0.0f, 0.0f);
		float QueryRadius = 0.0f;
		if (!BuildLocalShadowLightQuerySphere(Request, QueryCenter, QueryRadius))
		{
			continue;
		}

		LocalLightShadowPrimitiveScratch.clear();
		World->GetSpatialIndex().SphereQueryPrimitives(
			QueryCenter,
			QueryRadius,
			LocalLightShadowPrimitiveScratch,
			SphereQueryScratch);

		for (UPrimitiveComponent* Primitive : LocalLightShadowPrimitiveScratch)
		{
			FScenePrimitiveEntry* Entry = FindPrimitiveEntry(Primitive);
			if (Entry == nullptr)
			{
				continue;
			}

			RefreshPrimitiveEntry(*Entry);
			if (!ShouldIncludePrimitive(*Entry, WorldType, bIncludeEditorOnlyPrimitives) ||
				!IsShadowRenderablePrimitive(Primitive))
			{
				continue;
			}

			if (!CollectedPrimitives.insert(Primitive).second)
			{
				continue;
			}

			Collector.CollectSceneShadowCaster(
				Primitive,
				ShowFlags,
				ViewMode,
				RenderBus,
				WorldType,
				bIncludeEditorOnlyPrimitives);
		}
	}
}

FScenePrimitiveEntry* FScene::FindPrimitiveEntry(UPrimitiveComponent* Primitive)
{
	const auto It = PrimitiveToEntryIndex.find(Primitive);
	if (It == PrimitiveToEntryIndex.end())
	{
		return nullptr;
	}

	const int32 EntryIndex = It->second;
	if (EntryIndex < 0 || EntryIndex >= static_cast<int32>(PrimitiveEntries.size()))
	{
		return nullptr;
	}

	return &PrimitiveEntries[EntryIndex];
}

const FScenePrimitiveEntry* FScene::FindPrimitiveEntry(UPrimitiveComponent* Primitive) const
{
	const auto It = PrimitiveToEntryIndex.find(Primitive);
	if (It == PrimitiveToEntryIndex.end())
	{
		return nullptr;
	}

	const int32 EntryIndex = It->second;
	if (EntryIndex < 0 || EntryIndex >= static_cast<int32>(PrimitiveEntries.size()))
	{
		return nullptr;
	}

	return &PrimitiveEntries[EntryIndex];
}

FSceneLightEntry* FScene::FindLightEntry(ULightComponentBase* Light)
{
	const auto It = LightToEntryIndex.find(Light);
	if (It == LightToEntryIndex.end())
	{
		return nullptr;
	}

	const int32 EntryIndex = It->second;
	if (EntryIndex < 0 || EntryIndex >= static_cast<int32>(LightEntries.size()))
	{
		return nullptr;
	}

	return &LightEntries[EntryIndex];
}

const FSceneLightEntry* FScene::FindLightEntry(ULightComponentBase* Light) const
{
	const auto It = LightToEntryIndex.find(Light);
	if (It == LightToEntryIndex.end())
	{
		return nullptr;
	}

	const int32 EntryIndex = It->second;
	if (EntryIndex < 0 || EntryIndex >= static_cast<int32>(LightEntries.size()))
	{
		return nullptr;
	}

	return &LightEntries[EntryIndex];
}

void FScene::RefreshPrimitiveEntry(FScenePrimitiveEntry& Entry)
{
	UPrimitiveComponent* Primitive = Entry.Component;
	if (Primitive == nullptr)
	{
		return;
	}

	Entry.PrimitiveType = Primitive->GetPrimitiveType();
	Entry.bVisible = Primitive->IsVisible();
	Entry.bEditorOnly = Primitive->IsEditorOnly();
	Entry.bEnableCull = Primitive->IsEnableCull();
}

void FScene::RefreshLightEntry(FSceneLightEntry& Entry)
{
	ULightComponentBase* Light = Entry.Component;
	if (Light == nullptr)
	{
		return;
	}

	Entry.bEditorOnly = Light->IsEditorOnly();
}

std::unique_ptr<FPrimitiveRenderProxy> FScene::CreatePrimitiveProxy(UPrimitiveComponent* Primitive)
{
	if (Primitive == nullptr)
	{
		return nullptr;
	}

	if (UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(Primitive))
	{
		return std::make_unique<FParticleSystemSceneProxy>(ParticleComponent);
	}

	switch (Primitive->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_StaticMesh:
		return std::make_unique<FStaticMeshSceneProxy>(Primitive);
	case EPrimitiveType::EPT_SkeletalMesh:
		return std::make_unique<FSkeletalMeshSceneProxy>(Primitive);
	case EPrimitiveType::EPT_ProceduralMesh:
		return std::make_unique<FProceduralMeshSceneProxy>(Primitive);
	case EPrimitiveType::EPT_Billboard:
		return std::make_unique<FBillboardSceneProxy>(Primitive);
	case EPrimitiveType::EPT_Text:
		return std::make_unique<FTextSceneProxy>(Primitive);
	case EPrimitiveType::EPT_SubUV:
		return std::make_unique<FSubUVSceneProxy>(Primitive);
	case EPrimitiveType::EPT_Decal:
		return std::make_unique<FDecalSceneProxy>(Primitive);
	default:
		break;
	}

	if (FPrimitiveRenderProxy* Proxy = Primitive->CreateSceneProxy())
	{
		return std::unique_ptr<FPrimitiveRenderProxy>(Proxy);
	}

	return nullptr;
}

bool FScene::IsPrimitiveVisibleForScene(const FScenePrimitiveEntry& Entry) const
{
	UPrimitiveComponent* Primitive = Entry.Component;
	if (Primitive == nullptr || !Entry.bVisible)
	{
		return false;
	}

	const AActor* Owner = Primitive->GetOwner();
	return Owner == nullptr || Owner->IsVisible();
}

bool FScene::ShouldIncludePrimitive(const FScenePrimitiveEntry& Entry, EWorldType WorldType,
                                    bool bIncludeEditorOnlyPrimitives) const
{
	if (!IsPrimitiveVisibleForScene(Entry))
	{
		return false;
	}

	if (!bIncludeEditorOnlyPrimitives && Entry.bEditorOnly && WorldType != EWorldType::Editor)
	{
		return false;
	}

	return true;
}

bool FScene::ShouldIncludeLight(const FSceneLightEntry& Entry, EWorldType WorldType,
                                bool bIncludeEditorOnlyPrimitives) const
{
	ULightComponentBase* Light = Entry.Component;
	if (Light == nullptr)
	{
		return false;
	}

	const AActor* Owner = Light->GetOwner();
	if (Owner != nullptr && !Owner->IsVisible())
	{
		return false;
	}

	if (!bIncludeEditorOnlyPrimitives && Entry.bEditorOnly && WorldType != EWorldType::Editor)
	{
		return false;
	}

	return true;
}

bool FScene::ShouldMarkSpatialBoundsDirty(ESceneProxyDirtyFlag DirtyFlag) const
{
	const uint32 DirtyMask = ToSceneProxyDirtyMask(DirtyFlag);
	const uint32 SpatialMask =
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Transform) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Visibility) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Culling) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Mesh) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::ParticleTemplate) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Text) |
		ToSceneProxyDirtyMask(ESceneProxyDirtyFlag::Decal);
	return (DirtyMask & SpatialMask) != 0u;
}
