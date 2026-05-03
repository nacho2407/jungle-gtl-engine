#pragma once

#include "Component/SceneComponent.h"

class AActor;
class UCameraComponent;

class UCameraBoomComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UCameraBoomComponent, USceneComponent)

    UCameraBoomComponent() = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    
    AActor* GetTargetActor() const { return TargetActor; }
    const FString& GetTargetActorName() const { return TargetActorName; }
    //-----Seperate with logic
    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void SetTargetActor(AActor* InTargetActor);

private:
    AActor* ResolveTargetActor();
    UCameraComponent* FindControlledCamera() const;
    void ApplyMouseOrbitInput();
    bool ConsumeExternalCameraLocation(const FVector& TargetLocation);
    void UpdateCamera(float DeltaTime, bool bSnapToTarget);

private:
    AActor* TargetActor = nullptr;
    FString TargetActorName = "None";

    float TargetDistance = 100.0f;
    FVector OffsetDirection = FVector(-1.0f, 0.0f, 0.35f);
    bool bEnableMouseOrbit = true;
    bool bRequireRightMouseForOrbit = true;
    float MouseOrbitSensitivity = 0.3f;
    float MinPitchDegrees = -80.0f;
    float MaxPitchDegrees = 80.0f;

    FVector LastAppliedCameraLocationByBoom = FVector(0.0f, 0.0f, 0.0f);
    bool bHasLastAppliedCameraLocation = false;
};
