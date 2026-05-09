#pragma once
#include "SkinnedMeshComponent.h"
#include "Render/Resource/VertexTypes.h"
#include "Asset/SkeletalMesh.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    /* Tick */
    void TickComponent(float DeltaTime);

    /* Update Pipeline */
	// 미구현 상태
    void UpdateBoneTransforms() {}; // Local → Global
    void UpdateSkinningMatrices() {}; // Global → Skinning
    void SkinVerticesCPU() {};        // Optional

    /* Pose */
    const TArray<FMatrix>& GetLocalPose() const { return CurrentLocalPose; }
    const TArray<FMatrix>& GetGlobalPose() const { return CurrentGlobalPose; }

    /* Skinning */
    const TArray<FMatrix>& GetSkinningMatrices() const { return SkinningMatrices; }

    /* CPU Skinning Result (optional) */
    const TArray<FSkeletalMeshVertex>& GetSkinnedVertices() const { return SkinnedVertices; }

    bool IsCPUSkinningEnabled() const { return bEnableCPUSkinning; }
    void SetCPUSkinning(bool bEnable) { bEnableCPUSkinning = bEnable; }

private:
    USkeletalMesh* SkeletalMesh = nullptr;

    /* Pose */
    TArray<FMatrix> CurrentLocalPose;
    TArray<FMatrix> CurrentGlobalPose;

    /* Skinning */
    TArray<FMatrix> SkinningMatrices;

    /* CPU Skinning buffer (optional) */
    TArray<FSkeletalMeshVertex> SkinnedVertices;

	// 발제 내용 상 true 로 고정
    bool bEnableCPUSkinning = true;
};