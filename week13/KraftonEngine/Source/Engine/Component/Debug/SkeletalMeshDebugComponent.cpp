#include "SkeletalMeshDebugComponent.h"

#include "Core/Logging/Log.h"

#include <algorithm>

void USkeletalMeshDebugComponent::StopRagdollPreviewSimulation()
{
	ForceStopRagdollWithoutRecovery();
}

void USkeletalMeshDebugComponent::TickRagdollPreviewSimulation(float DeltaTime)
{
	if (bRagdollActive)
	{
		TickRagdollPhysicsMode(DeltaTime);
	}
}

void USkeletalMeshDebugComponent::SetRagdollPreviewLocalPose(const TArray<FTransform>& LocalPose)
{
	SetBoneLocalTransformsDirect(LocalPose);
}

void USkeletalMeshDebugComponent::SetRagdollGravityEnabled(bool bEnableGravity)
{
	bPreviewRagdollGravityEnabled = bEnableGravity;
	SetAllRagdollBodiesGravityEnabled(bPreviewRagdollGravityEnabled);
}

void USkeletalMeshDebugComponent::SetRagdollCreateConstraints(bool bCreateConstraints)
{
	if (bCreateRagdollConstraints == bCreateConstraints)
	{
		return;
	}

	bCreateRagdollConstraints = bCreateConstraints;
	if (!bRagdollActive)
	{
		return;
	}

	if (!bCreateRagdollConstraints)
	{
		DestroyRagdollConstraints();
		return;
	}

	if (Constraints.empty() && !CreateRagdollConstraintsFromPhysicsAsset())
	{
		UE_LOG("SetRagdollCreateConstraints warning: no ragdoll constraints created");
	}
}

void USkeletalMeshDebugComponent::SetRagdollSelfCollisionMode(ERagdollSelfCollisionMode InMode)
{
	if (RagdollSelfCollisionMode == InMode)
	{
		return;
	}

	RagdollSelfCollisionMode = InMode;
	if (!bRagdollActive || !bCreateRagdollConstraints)
	{
		return;
	}

	DestroyRagdollConstraints();
	if (!CreateRagdollConstraintsFromPhysicsAsset())
	{
		UE_LOG("SetRagdollSelfCollisionMode warning: no ragdoll constraints created");
	}
}

void USkeletalMeshDebugComponent::SetRagdollGlobalPhysicsBlendWeight(float InPhysicsBlendWeight)
{
	RagdollPhysicsBlendWeight = std::clamp(InPhysicsBlendWeight, 0.0f, 1.0f);
}
