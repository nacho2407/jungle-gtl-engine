#include "RenderCollector.h"

#include "Component/PrimitiveComponent.h"
#include "Render/Scene/PrimitiveRenderProxy.h"
#include "Render/Scene/Scene.h"

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

void FRenderCollector::ResetParticleStats()
{
	LastParticleStats = {};
}

void FRenderCollector::BeginSceneCollection(FRenderResourceProvider& ResourceProvider)
{
	ActiveResourceProvider = &ResourceProvider;
	ResetCullingStats();
	ResetDecalStats();
	ResetLightStats();
	ResetParticleStats();
}

void FRenderCollector::NoteVisiblePrimitive()
{
	++LastCullingStats.TotalVisiblePrimitiveCount;
}

void FRenderCollector::NoteBVHPassedPrimitive()
{
	++LastCullingStats.BVHPassedPrimitiveCount;
}

void FRenderCollector::NoteFallbackPassedPrimitive()
{
	++LastCullingStats.FallbackPassedPrimitiveCount;
}

void FRenderCollector::CollectScenePrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                             FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (Primitive == nullptr || ActiveResourceProvider == nullptr)
	{
		return;
	}

	FScene* Scene = Primitive->GetRegisteredScene();
	FPrimitiveRenderProxy* RenderProxy = Scene != nullptr ? Scene->GetPrimitiveRenderProxy(Primitive) : nullptr;
	if (RenderProxy == nullptr)
	{
		return;
	}

	FPrimitiveRenderProxyCommandServices CommandServices{
		PrimitiveDrawCommandBuilder,
		DecalCommandBuilder,
		OBBQueryScratch,
		LastParticleStats
	};
	FPrimitiveRenderProxyCollectionContext ProxyContext{
		RenderBus,
		*ActiveResourceProvider,
		CommandServices,
		ShowFlags,
		ViewMode,
		WorldType,
		bIncludeEditorOnlyPrimitives,
		EPrimitiveRenderProxyCollectIntent::Normal,
		Device,
		DeviceContext
	};
	RenderProxy->CollectCommands(ProxyContext);
}

void FRenderCollector::CollectSceneShadowCaster(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                                FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives)
{
	if (Primitive == nullptr || ActiveResourceProvider == nullptr)
	{
		return;
	}

	FScene* Scene = Primitive->GetRegisteredScene();
	FPrimitiveRenderProxy* RenderProxy = Scene != nullptr ? Scene->GetPrimitiveRenderProxy(Primitive) : nullptr;
	if (RenderProxy == nullptr)
	{
		return;
	}

	FPrimitiveRenderProxyCommandServices CommandServices{
		PrimitiveDrawCommandBuilder,
		DecalCommandBuilder,
		OBBQueryScratch,
		LastParticleStats
	};
	FPrimitiveRenderProxyCollectionContext ProxyContext{
		RenderBus,
		*ActiveResourceProvider,
		CommandServices,
		ShowFlags,
		ViewMode,
		WorldType,
		bIncludeEditorOnlyPrimitives,
		EPrimitiveRenderProxyCollectIntent::ShadowOnly,
		Device,
		DeviceContext
	};
	RenderProxy->CollectCommands(ProxyContext);
}

void FRenderCollector::CollectSceneLight(const ULightComponentBase* Light, FRenderBus& RenderBus)
{
	LightRenderCollector.CollectLight(Light, RenderBus);
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags,
                                        EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives)
{
	if (ActiveResourceProvider == nullptr)
	{
		return;
	}

	EditorOverlayCollector.CollectSelection(
		SelectedActors,
		ShowFlags,
		ViewMode,
		RenderBus,
		*ActiveResourceProvider,
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
	if (ActiveResourceProvider == nullptr)
	{
		return;
	}

	EditorOverlayCollector.CollectGizmo(Gizmo, ShowFlags, RenderBus, *ActiveResourceProvider, bIsActiveOperation);
}
