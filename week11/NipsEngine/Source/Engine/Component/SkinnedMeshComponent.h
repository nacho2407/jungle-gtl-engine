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
    bool HasValidMesh() const;

    const TArray<FMatrix>& GetCurrentLocalPose() const { return CurrentLocalPose; }
    const TArray<FMatrix>& GetCurrentGlobalPose() const { return CurrentGlobalPose; }
    const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }
    const TArray<FNormalVertex>& GetSkinnedVertices() const { return SkinnedVertices; }

    void MarkSkinningDirty();
    void EnsureSkinningUpdated();

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

    bool ConsumeRenderStateDirty();

protected:
    void InitializePoseFromBindPose();
    void UpdateCurrentGlobalPose();
    void UpdateSkinningMatrices();
    void SkinVerticesCPU();

    void MarkBoundsDirty();
    void MarkRenderStateDirty();
    void EnsureBoundsUpdated() const;

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
