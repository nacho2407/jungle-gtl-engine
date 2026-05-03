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
    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void SetTargetActor(AActor* InTargetActor);
    AActor* GetTargetActor() const { return TargetActor; }
    const FString& GetTargetActorName() const { return TargetActorName; }

private:
    AActor* ResolveTargetActor();
    AActor* FindFallbackTargetActor() const;
    UCameraComponent* FindControlledCamera() const;
    bool ConsumeExternalCameraLocation(const FVector& TargetLocation);
    void UpdateCamera(float DeltaTime, bool bSnapToTarget);

private:
    AActor* TargetActor = nullptr;
    FString TargetActorName = "None";

    float TargetDistance = 500.0f;
    FVector OffsetDirection = FVector(-1.0f, 0.0f, 0.35f);
    bool bAutoFindTargetIfUnset = true;

    FVector LastAppliedCameraLocation = FVector(0.0f, 0.0f, 0.0f);
    bool bHasLastAppliedCameraLocation = false;
};
