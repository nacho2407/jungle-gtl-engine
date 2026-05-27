#pragma once

#include <memory>

#include "Render/Scene/PrimitiveRenderProxy.h"
#include "Render/Common/ViewTypes.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Spatial/WorldSpatialIndex.h"

class AActor;
class FRenderBus;
class FRenderCollector;
class ULightComponentBase;
class UPrimitiveComponent;
class UWorld;
enum class EWorldType : uint32;
struct FFrustum;
struct ID3D11Device;
struct ID3D11DeviceContext;

enum class ESceneProxyDirtyFlag : uint32
{
	None = 0,
	Transform = 1u << 0,
	Visibility = 1u << 1,
	Culling = 1u << 2,
	Material = 1u << 3,
	Mesh = 1u << 4,
	ParticleTemplate = 1u << 5,
	Text = 1u << 6,
	Decal = 1u << 7,
	Resource = 1u << 8,
};

inline uint32 ToSceneProxyDirtyMask(ESceneProxyDirtyFlag Flag)
{
	return static_cast<uint32>(Flag);
}

struct FScenePrimitiveEntry
{
	UPrimitiveComponent* Component = nullptr;
	std::unique_ptr<FPrimitiveRenderProxy> Proxy;
	EPrimitiveType PrimitiveType = EPrimitiveType::MAX;
	bool bVisible = false;
	bool bEditorOnly = false;
	bool bEnableCull = true;
	uint32 DirtyFlags = 0u;
};

struct FSceneLightEntry
{
	ULightComponentBase* Component = nullptr;
	bool bEditorOnly = false;
};

class FSceneRenderResources final : public FRenderResourceProvider
{
public:
	void Initialize(ID3D11Device* InDevice);
	void Release();
	bool IsInitialized() const { return Device != nullptr; }

	FMeshBuffer& GetMeshBuffer(EPrimitiveType InPrimitiveType) override;
	FMeshBuffer* GetStaticMeshBuffer(const UStaticMesh* StaticMeshAsset, int32 LODLevel = 0) override;
	FMeshBuffer* GetProcMeshBuffer(uint32 ProcMeshCompUUID, const TArray<FNormalVertex>& Vertices, const TArray<uint32>& Indices) override;
	FMeshBuffer* GetCPUSkeletalMeshBuffer(uint32 SkeletalMeshCompUUID, const USkeletalMesh* SkeletalMeshAsset,
	                                      const TArray<FSkeletalMeshVertex>& Vertices, const TArray<uint32>& Indices,
	                                      bool bNeedsUpload) override;
	FMeshBuffer* GetGPUSkeletalMeshBuffer(const USkeletalMesh* SkeletalMeshAsset) override;
	FConstantBuffer* GetGPUSkeletalBoneMatrixBuffer(uint32 SkeletalMeshCompUUID, const FBoneMatrixConstants& Constants,
	                                                bool bNeedsUpload) override;

private:
	ID3D11Device* Device = nullptr;
	FMeshBufferManager MeshBufferManager;
};

class FScene
{
public:
	explicit FScene(UWorld* InWorld = nullptr);

	void SetWorld(UWorld* InWorld) { World = InWorld; }
	UWorld* GetWorld() const { return World; }

	void Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext);
	void Release();
	FSceneRenderResources& GetRenderResources() { return RenderResources; }
	const FSceneRenderResources& GetRenderResources() const { return RenderResources; }
	void Clear();
	void Rebuild(UWorld* InWorld);

	void RegisterActor(AActor* Actor);
	void UnregisterActor(AActor* Actor);
	void RegisterPrimitive(UPrimitiveComponent* Primitive);
	void UnregisterPrimitive(UPrimitiveComponent* Primitive);
	void RegisterLight(ULightComponentBase* Light);
	void UnregisterLight(ULightComponentBase* Light);
	void MarkPrimitiveDirty(UPrimitiveComponent* Primitive, ESceneProxyDirtyFlag DirtyFlag);
	FPrimitiveRenderProxy* GetPrimitiveRenderProxy(UPrimitiveComponent* Primitive);
	const FPrimitiveRenderProxy* GetPrimitiveRenderProxy(UPrimitiveComponent* Primitive) const;

	void CollectView(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                 FRenderBus& RenderBus, const FFrustum* ViewFrustum = nullptr,
	                 bool bIncludeEditorOnlyPrimitives = false);

	int32 GetPrimitiveCount() const { return static_cast<int32>(PrimitiveEntries.size()); }
	int32 GetLightCount() const { return static_cast<int32>(LightEntries.size()); }
	int32 GetDirtyPrimitiveCount() const { return DirtyPrimitiveCount; }

private:
	FScenePrimitiveEntry* FindPrimitiveEntry(UPrimitiveComponent* Primitive);
	const FScenePrimitiveEntry* FindPrimitiveEntry(UPrimitiveComponent* Primitive) const;
	FSceneLightEntry* FindLightEntry(ULightComponentBase* Light);
	const FSceneLightEntry* FindLightEntry(ULightComponentBase* Light) const;
	void RefreshPrimitiveEntry(FScenePrimitiveEntry& Entry);
	void RefreshLightEntry(FSceneLightEntry& Entry);
	std::unique_ptr<FPrimitiveRenderProxy> CreatePrimitiveProxy(UPrimitiveComponent* Primitive);
	bool ShouldMarkSpatialBoundsDirty(ESceneProxyDirtyFlag DirtyFlag) const;
	bool IsPrimitiveVisibleForScene(const FScenePrimitiveEntry& Entry) const;
	bool ShouldIncludePrimitive(const FScenePrimitiveEntry& Entry, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives) const;
	bool ShouldIncludeLight(const FSceneLightEntry& Entry, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives) const;
	void CollectRegisteredLights(FRenderCollector& Collector, FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectFullScanPrimitives(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                               FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectFrustumPrimitives(FRenderCollector& Collector, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
	                              EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType,
	                              bool bIncludeEditorOnlyPrimitives);
	void CollectDirectionalShadowCasters(FRenderCollector& Collector, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                                     FRenderBus& RenderBus, EWorldType WorldType, bool bIncludeEditorOnlyPrimitives);
	void CollectLocalShadowCasters(FRenderCollector& Collector, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags,
	                               EViewMode ViewMode, FRenderBus& RenderBus, EWorldType WorldType,
	                               bool bIncludeEditorOnlyPrimitives);

	UWorld* World = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	FSceneRenderResources RenderResources;
	TArray<FScenePrimitiveEntry> PrimitiveEntries;
	TArray<FSceneLightEntry> LightEntries;
	TMap<UPrimitiveComponent*, int32> PrimitiveToEntryIndex;
	TMap<ULightComponentBase*, int32> LightToEntryIndex;
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
	FWorldSpatialIndex::FPrimitiveSphereQueryScratch SphereQueryScratch;
	TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
	TArray<UPrimitiveComponent*> LocalLightShadowPrimitiveScratch;
	int32 DirtyPrimitiveCount = 0;
};
