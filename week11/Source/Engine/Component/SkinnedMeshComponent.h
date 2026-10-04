#pragma once

#include "SkinnedMeshComponent.generated.h"

#include "Asset/SkeletalMesh.h"
#include "Component/MeshComponent.h"
#include "Render/Resource/VertexTypes.h"

UCLASS()
class USkinnedMeshComponent : public UMeshComponent
{
    GENERATED_BODY_USkinnedMeshComponent()
public:
    DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

    USkinnedMeshComponent() = default;
    ~USkinnedMeshComponent() override = default;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void UpdateWorldAABB() const override;
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
    virtual const FAABB& GetWorldAABB() const;

    void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }
    bool HasValidMesh() const { return SkeletalMesh != nullptr && SkeletalMesh->HasValidMeshData(); }

    const TArray<FMatrix>& GetCurrentLocalPose() const { return CurrentLocalPose; }
    const TArray<FMatrix>& GetCurrentGlobalPose() const { return CurrentGlobalPose; }
    const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }
    const TArray<FNormalVertex>& GetSkinnedVertices() const { return SkinnedVertices; }

    // 본 i의 월드 변환 (component-space pose × actor world). 인덱스가 범위 밖이면 컴포넌트 월드 행렬을 반환.
    // 호출 측이 사전에 EnsurePoseUpdated를 보장
    FMatrix GetBoneWorldMatrix(int32 BoneIndex) const;

    bool ConsumeRenderStateDirty();
    void MarkPoseDirty()
    {
        bPoseDirty = true;
        bCPUSkinnedVerticesDirty = true;
    }

    void EnsurePoseUpdated();
    void EnsureCPUSkinnedVerticesUpdated();

    // Socket API override — mesh asset의 Sockets 정의를 사용.
    bool       HasSocket(const FName& SocketName) const override;
    FTransform GetSocketTransform(const FName& SocketName) const override;

protected:
    void OnTransformDirty() override;
    void InitializePoseFromBindPose();
    void UpdateCurrentGlobalPose();
    void UpdateSkinningMatrices();
    void SkinVerticesCPU();
    void MarkBoundsDirty() { bBoundsDirty = true; }

    void MarkRenderStateDirty() { bRenderStateDirty = true; }

protected:
    USkeletalMesh* SkeletalMesh = nullptr;
    UPROPERTY(EditAnywhere)
    FString SkeletalMeshPath;

    TArray<FMatrix> CurrentLocalPose;
    TArray<FMatrix> CurrentGlobalPose;
    TArray<FMatrix> SkinningMatrices;

    TArray<FNormalVertex> SkinnedVertices;

    bool bPoseDirty = true;
    bool bCPUSkinnedVerticesDirty = true;

    mutable bool bBoundsDirty = true;
    bool bRenderStateDirty = true;
};
