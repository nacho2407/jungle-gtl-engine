#include "BoneTransformGizmoTarget.h"

#include "Component/SkeletalMeshComponent.h"

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget()
	: MeshComponent(nullptr), BoneIndex(-1)
{
}

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget(USkeletalMeshComponent* InMeshComp, int32 InBoneIndex)
	: MeshComponent(InMeshComp), BoneIndex(InBoneIndex)
{
}

bool FBoneTransformGizmoTarget::IsValid() const
{
	return MeshComponent != nullptr && BoneIndex >= 0;
}

UWorld* FBoneTransformGizmoTarget::GetWorld() const
{
	return MeshComponent ? MeshComponent->GetWorld() : nullptr;
}

FVector FBoneTransformGizmoTarget::GetWorldLocation() const
{
	return MeshComponent ? MeshComponent->GetBoneLocationByIndex(BoneIndex) : FVector::ZeroVector;
}

FRotator FBoneTransformGizmoTarget::GetWorldRotation() const
{
	return MeshComponent ? MeshComponent->GetBoneRotationByIndex(BoneIndex) : FRotator::ZeroRotator;
}

FVector FBoneTransformGizmoTarget::GetWorldScale() const
{
	return MeshComponent ? MeshComponent->GetBoneScaleByIndex(BoneIndex) : FVector::OneVector;
}

void FBoneTransformGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
	// 본 트랜스폼은 직접 세팅이 불가능하므로 오프셋으로만 적용한다.
	if (MeshComponent)
	{
		FVector CurrentLocation = GetWorldLocation();
		FVector Delta = NewLocation - CurrentLocation;
		AddWorldOffset(Delta);
	}
}

void FBoneTransformGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
	if (MeshComponent)
	{
		FRotator CurrentRotation = GetWorldRotation();
		FQuat DeltaQuat = (NewRotation - CurrentRotation).ToQuaternion();
		AddWorldRotation(DeltaQuat, true);
	}
}

void FBoneTransformGizmoTarget::SetWorldScale(const FVector& NewScale)
{
	if (MeshComponent)
	{
		FVector CurrentScale = GetWorldScale();
		FVector Delta = NewScale - CurrentScale;
		AddScaleDelta(Delta);
	}
}

void FBoneTransformGizmoTarget::AddWorldOffset(const FVector& Delta)
{
	if (MeshComponent)
	{
		FVector CurrentLocation = GetWorldLocation();
		FVector NewLocation = CurrentLocation + Delta;
		MeshComponent->SetBoneLocationByIndex(BoneIndex, NewLocation);
	}
}

void FBoneTransformGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
	if (MeshComponent)
	{
		FRotator CurrentRotation = GetWorldRotation();
		FQuat NewRotation = Delta * FQuat::FromRotator(CurrentRotation);
		MeshComponent->SetBoneRotationByIndex(BoneIndex, NewRotation.ToRotator());
	}
}

void FBoneTransformGizmoTarget::AddScaleDelta(const FVector& Delta)
{
	if (MeshComponent)
	{
		FVector CurrentScale = GetWorldScale();
		FVector NewScale = CurrentScale + Delta;
		MeshComponent->SetBoneScaleByIndex(BoneIndex, NewScale);
	}
}
