#pragma once

#include "CoreMinimal.h"
#include "Component/PrimitiveComponent.h"
#include "Renderer/SceneRenderTypes.h"
#include <memory>

class FMaterial;
struct FRenderMesh;
class FRenderer;
class FObjectUniformStream;
struct FViewInfo;
class UStaticMeshComponent;
class UStaticMesh;
class UTextComponent;
class USubUVComponent;
class ULineBatchComponent;
class FDynamicMaterial;
struct FDynamicMesh;

class ENGINE_API FPrimitiveSceneProxy
{
public:
	explicit FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent);
	virtual ~FPrimitiveSceneProxy() = default;

	virtual void CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const = 0;
	virtual void CollectMeshBatchesForRenderMesh(const FViewInfo& View, FRenderMesh* InRenderMesh, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const;
	virtual bool TryBuildStaticMeshOcclusionCandidate(const FVector& CameraPosition, FRenderMesh*& OutRenderMesh, FStaticMeshOcclusionCandidate& OutCandidate) const;

	const FBoxSphereBounds& GetBounds() const { return Bounds; }

protected:
	FBoxSphereBounds Bounds;
};

class ENGINE_API FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent);
	void CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const override;
	void CollectMeshBatchesForRenderMesh(const FViewInfo& View, FRenderMesh* InRenderMesh, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const override;
	bool TryBuildStaticMeshOcclusionCandidate(const FVector& CameraPosition, FRenderMesh*& OutRenderMesh, FStaticMeshOcclusionCandidate& OutCandidate) const override;
	void BuildStaticMeshOcclusionCandidate(FStaticMeshOcclusionCandidate& OutCandidate) const;
	const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	FMaterial* GetMaterialForSection(int32 SectionIndex) const;
	FRenderMesh* ResolveRenderMesh(const FVector& CameraPosition) const;
	uint32 ResolveObjectUniformAllocation(FObjectUniformStream& ObjectUniformStream) const;
	uint32 EstimateDrawCallCount() const;

private:
	const UStaticMeshComponent* Component = nullptr;
	uint32 ComponentId = 0;
	UStaticMesh* StaticMesh = nullptr;
	FMatrix LocalToWorld = FMatrix::Identity;
	TArray<FMaterial*> Materials;
	FStaticMeshOcclusionCandidate CachedOcclusionCandidate;
	uint32 CachedDrawCallCount = 1;
	mutable uint32 CachedObjectUniformAllocation = UINT32_MAX;
};

class ENGINE_API FTextSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FTextSceneProxy(const UTextComponent* InComponent);
	~FTextSceneProxy() override;
	void CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const override;

private:
	FMatrix LocalToWorld = FMatrix::Identity;
	FVector RenderWorldPosition = FVector::ZeroVector;
	FVector RenderWorldScale = FVector::OneVector;
	FVector4 TextColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FString DisplayText;
	float TextScale = 1.0f;
	ERenderPass RenderPass = ERenderPass::Alpha;
	bool bBillboard = false;
	mutable bool bMeshDirty = true;
	mutable std::shared_ptr<FDynamicMesh> TextMesh;
	mutable std::unique_ptr<FDynamicMaterial> DynamicMaterial;
};

class ENGINE_API FSubUVSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FSubUVSceneProxy(const USubUVComponent* InComponent);
	~FSubUVSceneProxy() override;
	void CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const override;

private:
	FMatrix LocalToWorld = FMatrix::Identity;
	FVector2 Size = FVector2(1.0f, 1.0f);
	int32 Columns = 1;
	int32 Rows = 1;
	int32 TotalFrames = 1;
	int32 FirstFrame = 0;
	int32 LastFrame = 0;
	float FPS = 0.0f;
	bool bLoop = true;
	bool bBillboard = false;
	mutable bool bMeshDirty = true;
	mutable std::shared_ptr<FDynamicMesh> SubUVMesh;
	mutable std::unique_ptr<FDynamicMaterial> DynamicMaterial;
};

class ENGINE_API FLineBatchSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FLineBatchSceneProxy(const ULineBatchComponent* InComponent);
	void CollectMeshBatches(const FViewInfo& View, FRenderer& Renderer, TArray<FMeshRenderItem>& OutMeshBatches) const override;

private:
	FRenderMesh* RenderMesh = nullptr;
	FMatrix LocalToWorld = FMatrix::Identity;
};
