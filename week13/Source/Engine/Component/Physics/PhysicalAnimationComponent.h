#pragma once

#include "Component/ActorComponent.h"
#include "Math/Transform.h"
#include "Source/Engine/Component/Physics/PhysicalAnimationComponent.generated.h"

class USkeletalMeshComponent;
struct FBodyInstance;
struct FPoseContext;

USTRUCT()
struct FPhysicalAnimationDriveSettings
{
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    FName BoneName = FName::None;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    bool bDrivePosition = false;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    bool bDriveRotation = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float PositionStrength = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float PositionDamping = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float MaxForce = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float RotationStrength = 50.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float RotationDamping = 20.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation")
    float MaxTorque = 1500.0f;
};

struct FPhysicalAnimationBodyRuntimeData
{
    FName BoneName = FName::None;
    int32 BoneIndex = -1;
    FBodyInstance* Body = nullptr;
    FTransform BoneToBodyOffset;
};

UCLASS()
class UPhysicalAnimationComponent : public UActorComponent
{
public:
    GENERATED_BODY()

    UPhysicalAnimationComponent();

    void BeginPlay() override;

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void SetSkeletalMeshComponent(USkeletalMeshComponent* InMesh);

    // 연결되어 있는 SkeletalMeshComponent의 물리 애니메이션 모드 활성화
    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void ActivatePhysicalAnimation();

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void DeactivatePhysicalAnimation(bool bUseRecovery = true);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void StopDrivingKeepRagdoll();

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void PrePhysicsTick(float DeltaTime);

    UFUNCTION(Pure, Category = "Physics|PhysicalAnimation")
    bool IsPhysicalAnimationEnabled() const { return bPhysicalAnimationEnabled; }

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void SetDriveStrengthScale(float InScale);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void SetPhysicalAnimationSettings(FName BoneName, const FPhysicalAnimationDriveSettings& Settings);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void SetPhysicalAnimationSettingsBelow(FName BoneName, const FPhysicalAnimationDriveSettings& Settings, bool bIncludeSelf = true);

private:
    void AutoFindTargetMeshIfNeeded();
    void RebuildPhysicalAnimationRuntimeData();
    void ClearPhysicalAnimationRuntimeData();
    bool FindRuntimeDataForBody(FBodyInstance* Body, FPhysicalAnimationBodyRuntimeData*& OutRuntimeData);
    const FPhysicalAnimationDriveSettings* FindDriveSettingsForBone(FName BoneName) const;
    FPhysicalAnimationDriveSettings BuildCurrentDefaultDriveSettings() const;
    void ApplyDriveToBody(FBodyInstance* Body, const FTransform& TargetWorld, float DeltaTime, const FPhysicalAnimationDriveSettings& Settings);
    void ApplyLinearDrive(FBodyInstance* Body, const FTransform& TargetWorld,  float DeltaTime, const FPhysicalAnimationDriveSettings& Settings);
    void ApplyAngularDrive(FBodyInstance* Body, const FTransform& TargetWorld, float DeltaTime, const FPhysicalAnimationDriveSettings& Settings);
    void DebugDrawPhysicalAnimationTarget(FBodyInstance* Body, const FTransform& TargetWorld) const;
    FVector ComputeRotationError(const FQuat& Current, const FQuat& Target) const;

private:
    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Auto Find Skeletal Mesh")
    bool bAutoFindTargetMesh = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Enabled")
    bool bPhysicalAnimationEnabled = false;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Position")
    bool bDrivePosition = false;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Rotation")
    bool bDriveRotation = true;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Position Strength")
    float PositionStrength = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Position Damping")
    float PositionDamping = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Rotation Strength")
    float RotationStrength = 50.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Rotation Damping")
    float RotationDamping = 20.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Max Force")
    float MaxForce = 0.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Max Torque")
    float MaxTorque = 1500.0f;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Strength Scale")
    float DriveStrengthScale = 1.0f;

    // 처음에는 pelvis/root 하나만 위치 drive 하는 용도.
    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Drive Root Bone")
    FName DriveRootBoneName = FName::None;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation", DisplayName = "Per Bone Drive Settings")
    TArray<FPhysicalAnimationDriveSettings> PerBoneDriveSettings;

    UPROPERTY(Edit, Save, Category = "Physics|PhysicalAnimation|Debug", DisplayName = "Debug Draw")
    bool bDebugDraw = false;

    UPROPERTY(Transient, Category = "Physics|PhysicalAnimation")
    USkeletalMeshComponent* TargetMesh = nullptr;

    TArray<FPhysicalAnimationBodyRuntimeData> RuntimeBodies;
    bool bRuntimeBodiesDirty = true;

    TArray<FTransform> CachedAnimationLocalPose;
    TArray<FTransform> CachedAnimationWorldPose;
};
