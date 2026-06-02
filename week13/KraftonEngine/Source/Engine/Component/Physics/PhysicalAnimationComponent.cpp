#include "PhysicalAnimationComponent.h"

#include "Animation/PoseContext.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "Physics/BodyInstance.h"

#include <algorithm>
#include <cmath>

UPhysicalAnimationComponent::UPhysicalAnimationComponent()
{
    bTickEnable = false;
    PrimaryComponentTick.bCanEverTick = false;
    PrimaryComponentTick.bTickEnabled = false;
}

void UPhysicalAnimationComponent::SetSkeletalMeshComponent(USkeletalMeshComponent* InMesh)
{
    TargetMesh = InMesh;
    ClearPhysicalAnimationRuntimeData();
}

void UPhysicalAnimationComponent::ActivatePhysicalAnimation()
{
    AutoFindTargetMeshIfNeeded();

    if (!TargetMesh)
    {
        UE_LOG("PhysicalAnimation Activate failed: TargetMesh is null");
        return;
    }

    if (!TargetMesh->BeginPhysicalAnimation())
    {
        UE_LOG("PhysicalAnimation Activate failed: BeginPhysicalAnimation failed");
        return;
    }

    bPhysicalAnimationEnabled = true;

    RebuildPhysicalAnimationRuntimeData();

    UE_LOG("PhysicalAnimation activated");
}

void UPhysicalAnimationComponent::DeactivatePhysicalAnimation(bool bUseRecovery)
{
    bPhysicalAnimationEnabled = false;

    ClearPhysicalAnimationRuntimeData();

    if (TargetMesh)
    {
        TargetMesh->EndPhysicalAnimation(bUseRecovery);
    }

    UE_LOG("PhysicalAnimation deactivated");
}

void UPhysicalAnimationComponent::AutoFindTargetMeshIfNeeded()
{
    if (TargetMesh || !bAutoFindTargetMesh) return;

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return;

    TargetMesh = OwnerActor->GetComponentByClass<USkeletalMeshComponent>();
}

void UPhysicalAnimationComponent::PrePhysicsTick(float DeltaTime)
{
    if (!bPhysicalAnimationEnabled || DeltaTime <= 0.0f) return;

    AutoFindTargetMeshIfNeeded();

    if (!TargetMesh || !TargetMesh->IsPhysicalAnimationActive()) return;

    if (bRuntimeBodiesDirty || RuntimeBodies.empty())
    {
        RebuildPhysicalAnimationRuntimeData();
    }

    FPoseContext AnimPose;
    if (!TargetMesh->EvaluateAnimationPoseOnly(DeltaTime, AnimPose))
    {
        return;
    }

    CachedAnimationLocalPose = AnimPose.Pose;

    if (!TargetMesh->BuildWorldTransformsFromLocalPose(CachedAnimationLocalPose,CachedAnimationWorldPose))
    {
        return;
    }

    for (FPhysicalAnimationBodyRuntimeData& RuntimeData : RuntimeBodies)
    {
        FBodyInstance* Body = RuntimeData.Body;

        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        const int32 BoneIndex = RuntimeData.BoneIndex;
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CachedAnimationWorldPose.size()))
        {
            continue;
        }

        const FTransform TargetBoneWorld = CachedAnimationWorldPose[BoneIndex];
        const FTransform TargetBodyWorld = RuntimeData.BoneToBodyOffset * TargetBoneWorld;

        FPhysicalAnimationDriveSettings DriveSettings = BuildCurrentDefaultDriveSettings();
        if (const FPhysicalAnimationDriveSettings* OverrideSettings = FindDriveSettingsForBone(RuntimeData.BoneName))
        {
            DriveSettings = *OverrideSettings;
        }

        ApplyDriveToBody(Body, TargetBodyWorld, DeltaTime, DriveSettings);

        if (bDebugDraw)
        {
            DebugDrawPhysicalAnimationTarget(Body, TargetBodyWorld);
        }
    }
}

void UPhysicalAnimationComponent::SetDriveStrengthScale(float InScale)
{
    DriveStrengthScale = std::clamp(InScale, 0.0f, 10.0f);
}

void UPhysicalAnimationComponent::SetPhysicalAnimationSettings(
    FName BoneName,
    const FPhysicalAnimationDriveSettings& Settings)
{
    FPhysicalAnimationDriveSettings NewSettings = Settings;
    NewSettings.BoneName = BoneName;

    for (FPhysicalAnimationDriveSettings& Existing : PerBoneDriveSettings)
    {
        if (Existing.BoneName == BoneName)
        {
            Existing = NewSettings;
            return;
        }
    }

    PerBoneDriveSettings.push_back(NewSettings);
}

void UPhysicalAnimationComponent::SetPhysicalAnimationSettingsBelow(
    FName BoneName,
    const FPhysicalAnimationDriveSettings& Settings,
    bool bIncludeSelf)
{
    AutoFindTargetMeshIfNeeded();

    if (!TargetMesh)
    {
        return;
    }

    const TArray<FBodyInstance*>& Bodies = TargetMesh->GetRagdollBodies();
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || Body->BoneName == FName::None)
        {
            continue;
        }

        if (TargetMesh->IsBoneBelowBone(Body->BoneName, BoneName, bIncludeSelf))
        {
            SetPhysicalAnimationSettings(Body->BoneName, Settings);
        }
    }
}

void UPhysicalAnimationComponent::RebuildPhysicalAnimationRuntimeData()
{
    RuntimeBodies.clear();
    bRuntimeBodiesDirty = false;

    if (!TargetMesh)
    {
        return;
    }

    FPoseContext AnimPose;
    if (!TargetMesh->EvaluateAnimationPoseOnly(0.0f, AnimPose))
    {
        return;
    }

    TArray<FTransform> BoneWorldTransforms;
    if (!TargetMesh->BuildWorldTransformsFromLocalPose(AnimPose.Pose, BoneWorldTransforms))
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
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneWorldTransforms.size()))
        {
            continue;
        }

        const FTransform BoneWorld = BoneWorldTransforms[BoneIndex];
        const FTransform BodyWorld = Body->GetBodyTransform();

        FPhysicalAnimationBodyRuntimeData Data;
        Data.BoneName = Body->BoneName;
        Data.BoneIndex = BoneIndex;
        Data.Body = Body;
        Data.BoneToBodyOffset = FTransform::FromMatrixWithScale(
            BodyWorld.ToMatrix() * BoneWorld.ToMatrix().GetAffineInverse()
        );
        Data.BoneToBodyOffset.Scale = FVector::OneVector;

        RuntimeBodies.push_back(Data);
    }
}

void UPhysicalAnimationComponent::ClearPhysicalAnimationRuntimeData()
{
    RuntimeBodies.clear();
    bRuntimeBodiesDirty = true;
}

bool UPhysicalAnimationComponent::FindRuntimeDataForBody(
    FBodyInstance* Body,
    FPhysicalAnimationBodyRuntimeData*& OutRuntimeData)
{
    OutRuntimeData = nullptr;

    for (FPhysicalAnimationBodyRuntimeData& RuntimeData : RuntimeBodies)
    {
        if (RuntimeData.Body == Body)
        {
            OutRuntimeData = &RuntimeData;
            return true;
        }
    }

    return false;
}

const FPhysicalAnimationDriveSettings* UPhysicalAnimationComponent::FindDriveSettingsForBone(FName BoneName) const
{
    for (const FPhysicalAnimationDriveSettings& Settings : PerBoneDriveSettings)
    {
        if (Settings.BoneName == BoneName)
        {
            return &Settings;
        }
    }

    return nullptr;
}

FPhysicalAnimationDriveSettings UPhysicalAnimationComponent::BuildCurrentDefaultDriveSettings() const
{
    FPhysicalAnimationDriveSettings Settings;
    Settings.BoneName = FName::None;
    Settings.bDrivePosition = bDrivePosition;
    Settings.bDriveRotation = bDriveRotation;
    Settings.PositionStrength = PositionStrength;
    Settings.PositionDamping = PositionDamping;
    Settings.MaxForce = MaxForce;
    Settings.RotationStrength = RotationStrength;
    Settings.RotationDamping = RotationDamping;
    Settings.MaxTorque = MaxTorque;
    return Settings;
}

void UPhysicalAnimationComponent::ApplyDriveToBody(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime,
    const FPhysicalAnimationDriveSettings& Settings)
{
    if (!Body || !Body->IsValidBodyInstance()) return;

    bool bAppliedDrive = false;

    if (Settings.bDriveRotation && Settings.MaxTorque > 0.0f)
    {
        ApplyAngularDrive(Body, TargetWorld, DeltaTime, Settings);
        bAppliedDrive = true;
    }

    bool bShouldApplyPositionDrive = false;
    if (Settings.bDrivePosition && Settings.MaxForce > 0.0f)
    {
        if (Settings.BoneName != FName::None)
        {
            bShouldApplyPositionDrive = true;
        }
        else if (DriveRootBoneName != FName::None && TargetMesh &&
            TargetMesh->IsBoneBelowBone(Body->BoneName, DriveRootBoneName, true))
        {
            bShouldApplyPositionDrive = true;
        }
    }

    if (bShouldApplyPositionDrive)
    {
        ApplyLinearDrive(Body, TargetWorld, DeltaTime, Settings);
        bAppliedDrive = true;
    }

    if (bAppliedDrive)
    {
        Body->WakeUp();
    }
}

void UPhysicalAnimationComponent::ApplyLinearDrive(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime,
    const FPhysicalAnimationDriveSettings& Settings)
{
    (void)DeltaTime;

    const float EffectiveMaxForce = Settings.MaxForce * DriveStrengthScale;

    if (!Body || EffectiveMaxForce <= 0.0f)
    {
        return;
    }

    const FTransform CurrentWorld = Body->GetBodyTransform();

    FVector Error = TargetWorld.Location - CurrentWorld.Location;
    FVector Velocity = Body->GetLinearVelocity();

    const float EffectivePositionStrength = Settings.PositionStrength * DriveStrengthScale;
    const float EffectivePositionDamping = Settings.PositionDamping * DriveStrengthScale;

    FVector Force = Error * EffectivePositionStrength - Velocity * EffectivePositionDamping;

    const float ForceSize = Force.Length();
    if (ForceSize > EffectiveMaxForce && ForceSize > FMath::KINDA_SMALL_NUMBER)
    {
        Force = Force * (EffectiveMaxForce / ForceSize);
    }

    Body->AddForce(Force);
}

void UPhysicalAnimationComponent::ApplyAngularDrive(
    FBodyInstance* Body,
    const FTransform& TargetWorld,
    float DeltaTime,
    const FPhysicalAnimationDriveSettings& Settings)
{
    (void)DeltaTime;

    const float EffectiveMaxTorque = Settings.MaxTorque * DriveStrengthScale;

    if (!Body || EffectiveMaxTorque <= 0.0f)
    {
        return;
    }

    const FTransform CurrentWorld = Body->GetBodyTransform();

    FVector RotationError = ComputeRotationError(CurrentWorld.Rotation,TargetWorld.Rotation);
    FVector AngularVelocity = Body->GetAngularVelocity();

    const float EffectiveRotationStrength = Settings.RotationStrength * DriveStrengthScale;
    const float EffectiveRotationDamping = Settings.RotationDamping * DriveStrengthScale;

    FVector Torque = RotationError * EffectiveRotationStrength - AngularVelocity * EffectiveRotationDamping;

    const float TorqueSize = Torque.Length();
    if (TorqueSize > EffectiveMaxTorque && TorqueSize > FMath::KINDA_SMALL_NUMBER)
    {
        Torque = Torque * (EffectiveMaxTorque / TorqueSize);
    }

    Body->AddTorque(Torque);
}

void UPhysicalAnimationComponent::DebugDrawPhysicalAnimationTarget(
    FBodyInstance* Body,
    const FTransform& TargetWorld) const
{
    if (!Body || !Body->IsValidBodyInstance())
    {
        return;
    }

    const FTransform CurrentWorld = Body->GetBodyTransform();
    UWorld* World = GetWorld();

    DrawDebugPoint(World, CurrentWorld.Location, 5.0f, FColor::Yellow(), 0.0f);
    DrawDebugPoint(World, TargetWorld.Location, 5.0f, FColor::Green(), 0.0f);
    DrawDebugLine(World, CurrentWorld.Location, TargetWorld.Location, FColor::Blue(), 0.0f);
}

FVector UPhysicalAnimationComponent::ComputeRotationError(const FQuat& Current, const FQuat& Target) const
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
