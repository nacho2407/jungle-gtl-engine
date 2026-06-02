#pragma once

#include "Component/Primitive/SkeletalMeshComponent.h"

#include "Source/Engine/Component/Debug/SkeletalMeshDebugComponent.generated.h"

UCLASS()
class USkeletalMeshDebugComponent : public USkeletalMeshComponent
{
public:
	GENERATED_BODY()

	void StopRagdollPreviewSimulation();
	void TickRagdollPreviewSimulation(float DeltaTime);
	void SetRagdollPreviewLocalPose(const TArray<FTransform>& LocalPose);

	void SetRagdollGravityEnabled(bool bEnableGravity);
	bool IsRagdollGravityEnabled() const { return bPreviewRagdollGravityEnabled; }

	void SetRagdollCreateConstraints(bool bCreateConstraints);
	bool GetRagdollCreateConstraints() const { return bCreateRagdollConstraints; }

	void SetRagdollSelfCollisionMode(ERagdollSelfCollisionMode InMode);
	ERagdollSelfCollisionMode GetRagdollSelfCollisionMode() const { return RagdollSelfCollisionMode; }

	void SetRagdollGlobalPhysicsBlendWeight(float InPhysicsBlendWeight);
	float GetRagdollGlobalPhysicsBlendWeight() const { return RagdollPhysicsBlendWeight; }

private:
	bool bPreviewRagdollGravityEnabled = true;
};
