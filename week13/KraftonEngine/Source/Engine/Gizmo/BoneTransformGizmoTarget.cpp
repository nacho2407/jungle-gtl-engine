#include "BoneTransformGizmoTarget.h"

#include "Component/Primitive/SkeletalMeshComponent.h"

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget()
    : MeshComponent(nullptr), BoneIndex(-1)
{
}

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget(USkeletalMeshComponent* InMeshComp, int32 InBoneIndex)
    : MeshComponent(InMeshComp), BoneIndex(InBoneIndex)
{
}

void FBoneTransformGizmoTarget::SetBone(USkeletalMeshComponent* MeshComp, int32 InBoneIndex)
{
    MeshComponent = MeshComp;
    BoneIndex = InBoneIndex;
}

bool FBoneTransformGizmoTarget::IsValid() const
{
    return MeshComponent.IsValid() && BoneIndex >= 0;
}

UWorld* FBoneTransformGizmoTarget::GetWorld() const
{
    USkeletalMeshComponent* Comp = MeshComponent.Get();
    return Comp ? Comp->GetWorld() : nullptr;
}

FVector FBoneTransformGizmoTarget::GetWorldLocation() const
{
    USkeletalMeshComponent* Comp = MeshComponent.Get();
    return Comp ? Comp->GetBoneLocationByIndex(BoneIndex) : FVector::ZeroVector;
}

FRotator FBoneTransformGizmoTarget::GetWorldRotation() const
{
    USkeletalMeshComponent* Comp = MeshComponent.Get();
    return Comp ? Comp->GetBoneRotationByIndex(BoneIndex) : FRotator::ZeroRotator;
}

FQuat FBoneTransformGizmoTarget::GetWorldQuat() const
{
    USkeletalMeshComponent* Comp = MeshComponent.Get();
    return Comp ? Comp->GetBoneQuatByIndex(BoneIndex) : FQuat::Identity;
}

FVector FBoneTransformGizmoTarget::GetWorldScale() const
{
    USkeletalMeshComponent* Comp = MeshComponent.Get();
    return Comp ? Comp->GetBoneScaleByIndex(BoneIndex) : FVector::OneVector;
}

void FBoneTransformGizmoTarget::SetWorldLocation(const FVector& NewLocation)
{
    if (MeshComponent.IsValid())
    {
        FVector CurrentLocation = GetWorldLocation();
        FVector Delta = NewLocation - CurrentLocation;
        AddWorldOffset(Delta);
    }
}

void FBoneTransformGizmoTarget::SetWorldRotation(const FRotator& NewRotation)
{
    if (MeshComponent.IsValid())
    {
        FRotator CurrentRotation = GetWorldRotation();
        FQuat DeltaQuat = (NewRotation - CurrentRotation).ToQuaternion();
        AddWorldRotation(DeltaQuat, true);
    }
}

void FBoneTransformGizmoTarget::SetWorldRotation(const FQuat& NewQuat)
{
    if (MeshComponent.IsValid())
    {
        FQuat CurrentQuat = GetWorldQuat();
        FQuat DeltaQuat = NewQuat * CurrentQuat.Inverse();
        AddWorldRotation(DeltaQuat, true);
    }
}

void FBoneTransformGizmoTarget::SetWorldScale(const FVector& NewScale)
{
    if (MeshComponent.IsValid())
    {
        FVector CurrentScale = GetWorldScale();
        FVector Delta = NewScale - CurrentScale;
        AddScaleDelta(Delta);
    }
}

void FBoneTransformGizmoTarget::AddWorldOffset(const FVector& Delta)
{
    if (USkeletalMeshComponent* Comp = MeshComponent.Get())
    {
        FVector CurrentLocation = GetWorldLocation();
        FVector NewLocation = CurrentLocation + Delta;
        Comp->SetBoneLocationByIndex(BoneIndex, NewLocation);
    }
}

void FBoneTransformGizmoTarget::AddWorldRotation(const FQuat& Delta, bool bWorldSpace)
{
    if (USkeletalMeshComponent* Comp = MeshComponent.Get())
    {
        FQuat CurrentRotation = GetWorldQuat();
        FQuat NewRotation = bWorldSpace ? Delta * CurrentRotation : CurrentRotation * Delta;
        Comp->SetBoneRotationByIndex(BoneIndex, NewRotation);
    }
}

void FBoneTransformGizmoTarget::AddScaleDelta(const FVector& Delta)
{
    if (USkeletalMeshComponent* Comp = MeshComponent.Get())
    {
        FVector CurrentScale = GetWorldScale();
        FVector NewScale = CurrentScale + Delta;
        Comp->SetBoneScaleByIndex(BoneIndex, NewScale);
    }
}
