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
struct ID3D11Device;
struct ID3D11DeviceContext;

class FRenderCollector
{
public:
	struct FCullingStats
	{
		int32 TotalVisiblePrimitiveCount{ 0 };
		int32 BVHPassedPrimitiveCount{ 0 };
		int32 FallbackPassedPrimitiveCount{ 0 };
	};

	using FDecalStats = FRenderDecalStats;
	using FLightStats = FRenderLightStats;

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	FMeshBufferManager MeshBufferManager;
	FDecalCommandBuilder DecalCommandBuilder;
	FPrimitiveDrawCommandBuilder PrimitiveDrawCommandBuilder;

	FEditorOverlayCollector EditorOverlayCollector;
	FLightRenderCollector LightRenderCollector;

	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
	FWorldSpatialIndex::FPrimitiveOBBQueryScratch OBBQueryScratch;
	FWorldSpatialIndex::FPrimitiveSphereQueryScratch SphereQueryScratch;

	TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
	TArray<UPrimitiveComponent*> LocalLightShadowPrimitiveScratch;
	FCullingStats LastCullingStats;

public:
    // NOTE: Render Proxy 부분 도입으로 인해서 생긴 과도기적 형태
    // 최종적으로는 InDevice만 가지고 DeviceContext와 MeshBuffer는 몰라도 되게 바뀌어야 함
	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
	{
		Device = InDevice;
		DeviceContext = InDeviceContext;
		MeshBufferManager.Create(InDevice);
	}
	void Release()
	{
		MeshBufferManager.Release();
		Device = nullptr;
		DeviceContext = nullptr;
	}

	void CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus, const FFrustum* ViewFrustum = nullptr, bool bIncludeEditorOnlyPrimitives = false);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives = false);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic = false);
	void CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus);
	void CollectSingleBone(USkeletalMeshComponent* SkComp, int32 BoneIndex, FRenderBus& RenderBus);
	FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel = 0) { return MeshBufferManager.GetStaticMeshBuffer(StaticMeshAsset, LODLevel); }

	const FCullingStats& GetLastCullingStats() const { return LastCullingStats; }
	const FDecalStats& GetLastDecalStats() const { return DecalCommandBuilder.GetLastStats(); }
	const FLightStats& GetLastLightStats() const { return LightRenderCollector.GetLastStats(); }

private:
	void ResetCullingStats();
	void ResetDecalStats();
	void ResetLightStats();

	void CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                             FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives);
	void CollectDirectionalShadowCasters(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                                     FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives);
	void CollectLocalShadowCasters(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                               FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives);
	void CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                      EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                          FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectShadowCasterFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                                      FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectLight(const ULightComponentBase* Light, FRenderBus& RenderBus);
};
