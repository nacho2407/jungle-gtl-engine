#pragma once

#include "Component/ActorComponent.h"
#include "Math/Transform.h"
#include "Source/Engine/Component/Physics/PhysicalAnimationComponent.generated.h"

class USkeletalMeshComponent;
struct FBodyInstance;
struct FPoseContext;

UCLASS()
class UPhysicalAnimationComponent : public UActorComponent
{
public:
    GENERATED_BODY()

    UPhysicalAnimationComponent();

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void SetSkeletalMeshComponent(USkeletalMeshComponent* InMesh);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void ActivatePhysicalAnimation();

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void DeactivatePhysicalAnimation(bool bUseRecovery = true);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void PrePhysicsTick(float DeltaTime);

    UFUNCTION(Pure, Category = "Physics|PhysicalAnimation")
    bool IsPhysicalAnimationEnabled() const { return bPhysicalAnimationEnabled; }

private:
    void AutoFindTargetMeshIfNeeded();

    void ApplyDriveToBody(
        FBodyInstance* Body,
        const FTransform& TargetWorld,
        float DeltaTime
    );

    void ApplyLinearDrive(
        FBodyInstance* Body,
        const FTransform& TargetWorld,
        float DeltaTime
    );

    void ApplyAngularDrive(
        FBodyInstance* Body,
        const FTransform& TargetWorld,
        float DeltaTime
    );

    FVector ComputeRotationError(
        const FQuat& Current,
        const FQuat& Target
    ) const;

private:
    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Auto Find Skeletal Mesh")
    bool bAutoFindTargetMesh = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Enabled")
    bool bPhysicalAnimationEnabled = false;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Position")
    bool bDrivePosition = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Rotation")
    bool bDriveRotation = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Position Strength")
    float PositionStrength = 150.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Position Damping")
    float PositionDamping = 30.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Rotation Strength")
    float RotationStrength = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Rotation Damping")
    float RotationDamping = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Max Force")
    float MaxForce = 50000.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Max Torque")
    float MaxTorque = 50000.0f;

    // 처음에는 pelvis/root 하나만 위치 drive 하는 용도.
    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Root Bone")
    FName DriveRootBoneName = FName::None;

    UPROPERTY(Transient, Category = "Physics|PhysicalAnimation")
    USkeletalMeshComponent* TargetMesh = nullptr;

    TArray<FTransform> CachedAnimationLocalPose;
    TArray<FTransform> CachedAnimationWorldPose;
};