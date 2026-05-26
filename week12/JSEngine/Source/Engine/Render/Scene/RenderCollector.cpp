#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/PostProcess/Light/LightComponentBase.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Component/PostProcess/Light/SpotlightComponent.h"
#include "Engine/Geometry/Frustum.h"
#include "Render/Scene/PrimitiveRenderProxy.h"

#include <cmath>
#include <unordered_set>

namespace
{
FAABB BuildSphereAABB(const FVector& Center, float Radius);
bool BuildLocalShadowLightQuerySphere(const FShadowLightRequest& Request, FVector& OutCenter, float& OutRadius);
bool LocalShadowLightInfluenceIntersectsView(const FShadowLightRequest& Request, const FFrustum& ViewFrustum);
bool HasDirectionalShadowRequest(const FRenderBus& RenderBus);
bool IsShadowRenderablePrimitive(const UPrimitiveComponent* Primitive);

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

/*
 * BillBoardComponent를 상속받은 text, SubUV가 사용하는 AABB 계산함수(의존성 분리)
 */
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
} // namespace

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
									const FFrustum* ViewFrustum, bool bIncludeEditorOnlyPrimitives)
{
	ResetCullingStats();
	ResetDecalStats();
	ResetLightStats();

	if (!World)
		return;

	if (ViewFrustum != nullptr)
	{
		CollectWorldWithFrustum(World, *ViewFrustum, ShowFlags, ViewMode, RenderBus, bIncludeEditorOnlyPrimitives);
		return;
	}

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;

		if (!Actor || !Actor->IsVisible())
			continue;

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive != nullptr && Primitive->IsVisible())
			{
				++LastCullingStats.TotalVisiblePrimitiveCount;
			}
		}

		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
	}
}

void FRenderCollector::ResetCullingStats()
{
	LastCullingStats = {};
}

void FRenderCollector::ResetDecalStats()
{
	DecalCommandBuilder.Reset();
}

void FRenderCollector::ResetLightStats()
{
	LightRenderCollector.Reset();
}

void FRenderCollector::CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
											   EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	VisiblePrimitiveScratch.clear();
	World->GetSpatialIndex().FrustumQueryPrimitives(ViewFrustum, VisiblePrimitiveScratch, FrustumQueryScratch);

	for (UPrimitiveComponent* Primitive : VisiblePrimitiveScratch)
	{
		if (Primitive == nullptr || UsesCameraDependentRenderBounds(Primitive) || !Primitive->IsEnableCull())
		{
			continue;
		}

		++LastCullingStats.BVHPassedPrimitiveCount;
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
	}

	std::unordered_set<UPrimitiveComponent*> CollectedCameraDependentPrimitives;
	CollectedCameraDependentPrimitives.reserve(32);

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (Actor == nullptr || !Actor->IsVisible())
		{
			continue;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (ULightComponentBase* Light = Cast<ULightComponentBase>(Comp))
			{
				CollectLight(Light, RenderBus);
			}
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive == nullptr || !Primitive->IsVisible())
			{
				continue;
			}

			++LastCullingStats.TotalVisiblePrimitiveCount;

			const bool bIsCameraDependent = UsesCameraDependentRenderBounds(Primitive);
			const bool bIsUncullable = !Primitive->IsEnableCull();

			if (!bIsCameraDependent && !bIsUncullable)
			{
				continue;
			}

			if (!CollectedCameraDependentPrimitives.insert(Primitive).second)
			{
				continue;
			}

			if (bIsCameraDependent && !bIsUncullable &&
				ViewFrustum.Intersects(BuildRenderAABB(Primitive, RenderBus)) == FFrustum::EFrustumIntersectResult::Outside)
			{
				continue;
			}

			++LastCullingStats.FallbackPassedPrimitiveCount;
			CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, World->GetWorldType(), bIncludeEditorOnlyPrimitives);
		}
	}

	CollectDirectionalShadowCasters(World, ShowFlags, ViewMode, RenderBus, bIncludeEditorOnlyPrimitives);
	CollectLocalShadowCasters(World, ViewFrustum, ShowFlags, ViewMode, RenderBus, bIncludeEditorOnlyPrimitives);
}

void FRenderCollector::CollectDirectionalShadowCasters(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode,
													   FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	if (World == nullptr || RenderBus.ShadowLightRequests.empty() || !HasDirectionalShadowRequest(RenderBus))
	{
		return;
	}

	std::unordered_set<UPrimitiveComponent*> CollectedPrimitives;
	const TArray<FRenderCommand>& OpaqueCommands = RenderBus.GetCommands(ERenderPass::Opaque);
	const TArray<FRenderCommand>& ShadowCommands = RenderBus.GetShadowCasterCommands();
	CollectedPrimitives.reserve(OpaqueCommands.size() + ShadowCommands.size() + 64);

	for (const FRenderCommand& Command : OpaqueCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}

	for (const FRenderCommand& Command : ShadowCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}

	const EWorldType WorldType = World->GetWorldType();
	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (Actor == nullptr || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive == nullptr || !Primitive->IsVisible() || !IsShadowRenderablePrimitive(Primitive))
			{
				continue;
			}

			if (!CollectedPrimitives.insert(Primitive).second)
			{
				continue;
			}

			CollectShadowCasterFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
		}
	}
}

void FRenderCollector::CollectLocalShadowCasters(
	UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, 
	EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	if (World == nullptr || RenderBus.ShadowLightRequests.empty())
	{
		return;
	}

	std::unordered_set<UPrimitiveComponent*> CollectedPrimitives;
	const TArray<FRenderCommand>& OpaqueCommands = RenderBus.GetCommands(ERenderPass::Opaque);
	const TArray<FRenderCommand>& ShadowCommands = RenderBus.GetShadowCasterCommands();
	CollectedPrimitives.reserve(OpaqueCommands.size() + ShadowCommands.size() + 32);
	for (const FRenderCommand& Command : OpaqueCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}
	for (const FRenderCommand& Command : ShadowCommands)
	{
		if (Command.SourcePrimitive != nullptr)
		{
			CollectedPrimitives.insert(Command.SourcePrimitive);
		}
	}

	const EWorldType WorldType = World->GetWorldType();
	FWorldSpatialIndex& SpatialIndex = World->GetSpatialIndex();

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

		SpatialIndex.SphereQueryPrimitives(
			QueryCenter,
			QueryRadius,
			LocalLightShadowPrimitiveScratch,
			SphereQueryScratch);

		for (UPrimitiveComponent* Primitive : LocalLightShadowPrimitiveScratch)
		{
			if (Primitive == nullptr || !Primitive->IsVisible() || !IsShadowRenderablePrimitive(Primitive))
			{
				continue;
			}

			if (!bIncludeEditorOnlyPrimitives && Primitive->IsEditorOnly() && WorldType != EWorldType::Editor)
			{
				continue;
			}

			if (!CollectedPrimitives.insert(Primitive).second)
			{
				continue;
			}

			CollectShadowCasterFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
		}
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags,
										EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	EditorOverlayCollector.CollectSelection(
		SelectedActors,
		ShowFlags,
		ViewMode,
		RenderBus,
		MeshBufferManager,
		bIncludeEditorOnlyPrimitives);
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic)
{
	EditorOverlayCollector.CollectGrid(GridSpacing, GridHalfLineCount, RenderBus, bOrthographic);
}

void FRenderCollector::CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus)
{
	EditorOverlayCollector.CollectSkeletonBones(SkComp, RenderBus);
}

void FRenderCollector::CollectSingleBone(USkeletalMeshComponent* SkComp, int32 BoneIndex, FRenderBus& RenderBus)
{
	EditorOverlayCollector.CollectSingleBone(SkComp, BoneIndex, RenderBus);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation)
{
	EditorOverlayCollector.CollectGizmo(Gizmo, ShowFlags, RenderBus, MeshBufferManager, bIsActiveOperation);
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
										EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (!Actor->IsVisible())
		return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus, WorldType, bIncludeEditorOnlyPrimitives);
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
											FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (!Primitive->IsVisible())
		return;
	if (!bIncludeEditorOnlyPrimitives && Primitive->IsEditorOnly() && WorldType != EWorldType::Editor)
		return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();
	if (PrimType == EPrimitiveType::EPT_ParticleSystem && !ShowFlags.bParticle)
	{
		return;
	}

	if (FPrimitiveRenderProxy* RenderProxy = Primitive->GetRenderProxy())
	{
		FPrimitiveRenderProxyCollectionContext ProxyContext{
			RenderBus,
			Device,
			DeviceContext
		};
		RenderProxy->CollectCommands(ProxyContext);
		return;
	}

	switch (PrimType)
	{
	case EPrimitiveType::EPT_Decal:
		DecalCommandBuilder.CollectDecal(Primitive, ShowFlags, RenderBus, MeshBufferManager, OBBQueryScratch);
		break;
	default:
		PrimitiveDrawCommandBuilder.CollectPrimitive(Primitive, ShowFlags, ViewMode, RenderBus, MeshBufferManager);
		break;
	}
}

void FRenderCollector::CollectShadowCasterFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
														FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (Primitive == nullptr || !Primitive->IsVisible() || !IsShadowRenderablePrimitive(Primitive))
	{
		return;
	}
	if (!bIncludeEditorOnlyPrimitives && Primitive->IsEditorOnly() && WorldType != EWorldType::Editor)
	{
		return;
	}

	PrimitiveDrawCommandBuilder.CollectShadowCasterPrimitive(Primitive, ShowFlags, ViewMode, RenderBus, MeshBufferManager);
}

void FRenderCollector::CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus)
{
	LightRenderCollector.CollectLight(Light, RenderBus);
}
