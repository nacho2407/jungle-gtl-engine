#pragma once
#include "RenderBus.h"
#include "DecalCommandBuilder.h"
#include "EditorOverlayCollector.h"
#include "LightRenderCollector.h"
#include "PrimitiveDrawCommandBuilder.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Spatial/WorldSpatialIndex.h"

enum class EWorldType : uint32;

class UWorld;
class AActor;
class UPrimitiveComponent;
class UGizmoComponent;
class ULightComponentBase;
class USkeletalMeshComponent;
struct FFrustum;

class FRenderCollector {
public:
	struct FCullingStats
	{
		int32 TotalVisiblePrimitiveCount{0};
		int32 BVHPassedPrimitiveCount{0};
		int32 FallbackPassedPrimitiveCount{0};
	};

	using FDecalStats = FRenderDecalStats;
	using FLightStats = FRenderLightStats;

private:
	FMeshBufferManager MeshBufferManager;
	FDecalCommandBuilder DecalCommandBuilder;
	FEditorOverlayCollector EditorOverlayCollector;
	FPrimitiveDrawCommandBuilder PrimitiveDrawCommandBuilder;
	FLightRenderCollector LightRenderCollector;
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
	FWorldSpatialIndex::FPrimitiveOBBQueryScratch OBBQueryScratch;
	TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
	FCullingStats LastCullingStats;

public:
	void Initialize(ID3D11Device* InDevice) { MeshBufferManager.Create(InDevice); }
	void Release() { MeshBufferManager.Release(); }

	void CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                  const FFrustum* ViewFrustum = nullptr, bool bIncludeEditorOnlyPrimitives = false);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                      FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives = false);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic = false);
	void CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus);
	FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel = 0)
	{
		return MeshBufferManager.GetStaticMeshBuffer(StaticMeshAsset, LODLevel);
	}
	const FCullingStats& GetLastCullingStats() const { return LastCullingStats; }
	const FDecalStats& GetLastDecalStats() const { return DecalCommandBuilder.GetLastStats(); }
    const FLightStats& GetLastLightStats() const { return LightRenderCollector.GetLastStats(); }

private:
	void ResetCullingStats();
	void ResetDecalStats();
	void ResetLightStats();

	void CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                             FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives);
	void CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                      EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                          FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus);
};
