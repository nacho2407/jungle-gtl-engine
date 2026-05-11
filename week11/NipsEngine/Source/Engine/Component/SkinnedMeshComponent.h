#pragma once

#include "Asset/SkeletalMesh.h"
#include "Component/MeshComponent.h"
#include "Render/Resource/VertexTypes.h"

class USkinnedMeshComponent : public UMeshComponent
{
public:
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

    USkinnedMeshComponent() = default;
    ~USkinnedMeshComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    bool HasValidMesh() const { return SkeletalMesh != nullptr && SkeletalMesh->HasValidMeshData(); }

    const TArray<FMatrix>& GetCurrentLocalPose() const { return CurrentLocalPose; }
    const TArray<FMatrix>& GetCurrentGlobalPose() const { return CurrentGlobalPose; }
    const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }
    const TArray<FNormalVertex>& GetSkinnedVertices() const { return SkinnedVertices; }

    void MarkSkinningDirty() { bSkinningDirty = true; }

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	virtual const FAABB& GetWorldAABB() const;

    bool ConsumeRenderStateDirty();

    void EnsureSkinningUpdated();

protected:
    void InitializePoseFromBindPose();
    void UpdateCurrentGlobalPose();
    void UpdateSkinningMatrices();

	/**
	 * @brief CPU skinning 핵심 함수
	 */
    void SkinVerticesCPU();

    void MarkBoundsDirty() { bBoundsDirty = true; }
    void MarkRenderStateDirty() { bRenderStateDirty = true; }
    void EnsureBoundsUpdated() const;

	/**
	 * @brief CPU skinning에서는 FSkeletonMeshVertex를 입력받아 FNormalVector를 출력하므로,
	 *        influence가 없을 때 원본을 복사하여 FNormalVertex로 변환하는 함수가 필요함
	 */
    FNormalVertex ConvertToNormalVertexWithoutSkinning(const FSkeletalMeshVertex& Source) const;

protected:
    USkeletalMesh* SkeletalMesh = nullptr;
    FString SkeletalMeshPath;

    TArray<FMatrix> CurrentLocalPose;
    TArray<FMatrix> CurrentGlobalPose;
    TArray<FMatrix> SkinningMatrices;

    TArray<FNormalVertex> SkinnedVertices;

    bool bEnableCPUSkinning = true;
    bool bSkinningDirty = true;

    mutable bool bBoundsDirty = true;
    bool bRenderStateDirty = true;
};
