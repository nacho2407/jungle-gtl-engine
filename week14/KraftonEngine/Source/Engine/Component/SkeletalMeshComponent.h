#pragma once

#include "SkinnedMeshComponent.h"

// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	// Render access 섹션: SceneProxy
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void EnsureBoneEditPose();
	void ResetBoneEditPose();

	FVector GetBoneLocationByIndex(int32 BoneIndex) const;
	FRotator GetBoneRotationByIndex(int32 BoneIndex) const;
	FQuat GetBoneQuatByIndex(int32 BoneIndex) const;
	FVector GetBoneScaleByIndex(int32 BoneIndex) const;
	FTransform GetBoneLocalTransformByIndex(int32 BoneIndex) const;

	void SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FQuat& NewQuat);
	void SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale);
	void SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform);

	void GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void GetCurrentBoneGlobalMatrices(TArray<FMatrix>& OutGlobals) const;
	const TArray<FVertexPNCTT>& GetSkinnedVertices() const { return SkinnedVertices; }
	uint64 GetSkinnedRevision() const { return SkinnedRevision; }
	
protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void InitSkinningCache();
	void UpdateCPUSkinning();
	void BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const;

private:
	TArray<FMatrix> BoneEditLocalMatrices;
	bool bUseBoneEditPose = false;

	TArray<FVertexPNCTT> SkinnedVertices;
	uint64 SkinnedRevision = 0;
};
