#pragma once
#include "RenderBus.h"
#include "DecalCommandBuilder.h"
#include "EditorOverlayCollector.h"
#include "LightRenderCollector.h"
#include "Particle/ParticleTypes.h"
#include "PrimitiveDrawCommandBuilder.h"
#include "Spatial/WorldSpatialIndex.h"

enum class EWorldType : uint32;

class AActor;
class FRenderResourceProvider;
class UPrimitiveComponent;
class UGizmoComponent;
class ULightComponentBase;
class USkeletalMeshComponent;
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
	using FParticleStats = FParticleFrameStats;

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	FRenderResourceProvider* ActiveResourceProvider = nullptr;
	FDecalCommandBuilder DecalCommandBuilder;
	FPrimitiveDrawCommandBuilder PrimitiveDrawCommandBuilder;

	FEditorOverlayCollector EditorOverlayCollector;
	FLightRenderCollector LightRenderCollector;

	FWorldSpatialIndex::FPrimitiveOBBQueryScratch OBBQueryScratch;

	FCullingStats LastCullingStats;
	FParticleStats LastParticleStats;

public:
    // NOTE: Render Proxy 부분 도입으로 인해서 생긴 과도기적 형태
    // 최종적으로는 frame upload 경로가 분리되면 DeviceContext 의존도 제거해야 함
	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
	{
		Device = InDevice;
		DeviceContext = InDeviceContext;
	}
	void Release()
	{
		ActiveResourceProvider = nullptr;
		Device = nullptr;
		DeviceContext = nullptr;
	}

	void BeginSceneCollection(FRenderResourceProvider& ResourceProvider);
	void NoteVisiblePrimitive();
	void NoteBVHPassedPrimitive();
	void NoteFallbackPassedPrimitive();
	void CollectScenePrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                           FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectSceneShadowCaster(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                              FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectSceneLight(const ULightComponentBase* Light, FRenderBus& RenderBus);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus, bool bIncludeEditorOnlyPrimitives = false);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic = false);
	void CollectSkeletonBones(USkeletalMeshComponent* SkComp, FRenderBus& RenderBus);
	void CollectSingleBone(USkeletalMeshComponent* SkComp, int32 BoneIndex, FRenderBus& RenderBus);
	const FCullingStats& GetLastCullingStats() const { return LastCullingStats; }
	const FDecalStats& GetLastDecalStats() const { return DecalCommandBuilder.GetLastStats(); }
	const FLightStats& GetLastLightStats() const { return LightRenderCollector.GetLastStats(); }
	const FParticleStats& GetLastParticleStats() const { return LastParticleStats; }

private:
	void ResetCullingStats();
	void ResetDecalStats();
	void ResetLightStats();
	void ResetParticleStats();
};
