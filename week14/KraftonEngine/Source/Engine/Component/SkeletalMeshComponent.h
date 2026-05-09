#pragma once

#include "SkinnedMeshComponent.h"

#include "Math/Rotator.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void EnsureBoneEditPose();
	void ResetBoneEditPose();

	FVector GetBoneLocationByIndex(int32 BoneIndex) const;
	FRotator GetBoneRotationByIndex(int32 BoneIndex) const;
	FVector GetBoneScaleByIndex(int32 BoneIndex) const;

	void SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation);
	void SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale);

private:
	void BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const;

private:
	TArray<FMatrix> BoneEditLocalMatrices;
	bool bUseBoneEditPose = false;
};
