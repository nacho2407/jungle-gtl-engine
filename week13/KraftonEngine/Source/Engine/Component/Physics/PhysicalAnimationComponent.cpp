#include "PhysicalAnimationComponent.h"

#include "Animation/PoseContext.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Physics/BodyInstance.h"

#include <algorithm>
#include <cmath>

static float VectorLength(const FVector& V)
{
    return sqrtf(V.X * V.X + V.Y * V.Y + V.Z * V.Z);
}

UPhysicalAnimationComponent::UPhysicalAnimationComponent()
{
    bTickEnable = false;
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bTickEnabled = false;
}

void UPhysicalAnimationComponent::SetSkeletalMeshComponent(USkeletalMeshComponent* InMesh)
{
    TargetMesh = InMesh;
}

void UPhysicalAnimationComponent::ActivatePhysicalAnimation()
{
    AutoFindTargetMeshIfNeeded();

    if (!TargetMesh)
    {
        return;
    }

    if (!TargetMesh->BeginPhysicalAnimation())
    {
        return;
    }

    bPhysicalAnimationEnabled = true;
}

void UPhysicalAnimationComponent::DeactivatePhysicalAnimation(bool bUseRecovery)
{
    bPhysicalAnimationEnabled = false;

    if (TargetMesh)
    {
        TargetMesh->EndPhysicalAnimation(bUseRecovery);
    }
}

void UPhysicalAnimationComponent::AutoFindTargetMeshIfNeeded()
{
    if (TargetMesh || !bAutoFindTargetMesh)
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    TargetMesh = OwnerActor->GetComponentByClass<USkeletalMeshComponent>();
}

void UPhysicalAnimationComponent::PrePhysicsTick(float DeltaTime)
{
    if (!bPhysicalAnimationEnabled)
    {
        return;
    }

    AutoFindTargetMeshIfNeeded();

    if (!TargetMesh || !TargetMesh->IsPhysicalAnimationActive())
    {
        return;
    }

    FPoseContext AnimPose;
    if (!TargetMesh->EvaluateAnimationPoseOnly(DeltaTime, AnimPose))
    {
        return;
    }

    CachedAnimationLocalPose = AnimPose.Pose;

    if (!TargetMesh->BuildWorldTransformsFromLocalPose(
        CachedAnimationLocalPose,
        CachedAnimationWorldPose))
    {
        return;
    }

    const TArray<FBodyInstance*>& Bodies = TargetMesh->GetRagdollBodies();

    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        const int32 BoneIndex = Body->BoneIndex;
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CachedAnimationWorldPose.size()))
        {
            continue;
        }

        ApplyDriveToBody(Body, CachedAnimationWorldPose[BoneIndex], DeltaTime);
    }
}

void UPhysicalAnimationComponent::ApplyDriveToBody(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime)
{
    if (!Body)
    {
        return;
    }

    if (bDriveRotation)
    {
        ApplyAngularDrive(Body, TargetWorld, DeltaTime);
    }

    if (bDrivePosition)
    {
        const bool bIsRootDriveBody =
            DriveRootBoneName == FName::None ||
            Body->BoneName == DriveRootBoneName;

        if (bIsRootDriveBody)
        {
            ApplyLinearDrive(Body, TargetWorld, DeltaTime);
        }
    }

    Body->WakeUp();
}

void UPhysicalAnimationComponent::ApplyLinearDrive(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime)
{
    (void)DeltaTime;

    const FTransform CurrentWorld = Body->GetBodyTransform();

    FVector Error = TargetWorld.Location - CurrentWorld.Location;
    FVector Velocity = Body->GetLinearVelocity();

    FVector Force = Error * PositionStrength - Velocity * PositionDamping;

    const float ForceSize = VectorLength(Force);
    if (ForceSize > MaxForce && ForceSize > FMath::KINDA_SMALL_NUMBER)
    {
        Force = Force * (MaxForce / ForceSize);
    }

    Body->AddForce(Force);
}

void UPhysicalAnimationComponent::ApplyAngularDrive(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime)
{
    (void)DeltaTime;

    const FTransform CurrentWorld = Body->GetBodyTransform();

    FVector RotationError = ComputeRotationError(
        CurrentWorld.Rotation,
        TargetWorld.Rotation
    );

    FVector AngularVelocity = Body->GetAngularVelocity();

    FVector Torque = RotationError * RotationStrength - AngularVelocity * RotationDamping;

    const float TorqueSize = VectorLength(Torque);
    if (TorqueSize > MaxTorque && TorqueSize > FMath::KINDA_SMALL_NUMBER)
    {
        Torque = Torque * (MaxTorque / TorqueSize);
    }

    Body->AddTorque(Torque);
}

FVector UPhysicalAnimationComponent::ComputeRotationError(
    const FQuat& Current,
    const FQuat& Target) const
{
    FQuat Delta = Target * Current.Inverse();
    Delta.Normalize();

    if (Delta.W < 0.0f)
    {
        Delta.X *= -1.0f;
        Delta.Y *= -1.0f;
        Delta.Z *= -1.0f;
        Delta.W *= -1.0f;
    }

    const float ClampedW = std::clamp(Delta.W, -1.0f, 1.0f);
    const float Angle = 2.0f * acosf(ClampedW);

    const float SinHalfAngle = sqrtf(std::max(1.0f - ClampedW * ClampedW, 0.0f));

    FVector Axis;
    if (SinHalfAngle < 0.001f)
    {
        Axis = FVector(Delta.X, Delta.Y, Delta.Z);
    }
    else
    {
        Axis = FVector(
            Delta.X / SinHalfAngle,
            Delta.Y / SinHalfAngle,
            Delta.Z / SinHalfAngle
        );
    }

    return Axis * Angle;
}