#pragma once

#include "GizmoTransformTarget.h"

class USkeletalMeshComponent;

class FBoneTransformGizmoTarget : public IGizmoTransformTarget
{
public:
	FBoneTransformGizmoTarget();
	FBoneTransformGizmoTarget(USkeletalMeshComponent* InMeshComp, int32 InBoneIndex);
	~FBoneTransformGizmoTarget() override = default;

public:
	void SetBone(USkeletalMeshComponent* MeshComp, int32 BoneIndex) { MeshComponent = MeshComp; this->BoneIndex = BoneIndex; }

	bool IsValid() const override;
	UWorld* GetWorld() const override;

	FVector GetWorldLocation() const override;
	FRotator GetWorldRotation() const override;
	FVector GetWorldScale() const override;

	void SetWorldLocation(const FVector& NewLocation) override;
	void SetWorldRotation(const FRotator& NewRotation) override;
	void SetWorldScale(const FVector& NewScale) override;

	void AddWorldOffset(const FVector& Delta) override;
	void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) override;
	void AddScaleDelta(const FVector& Delta) override;

private:
	USkeletalMeshComponent* MeshComponent = nullptr;
	int32 BoneIndex = -1;
};
