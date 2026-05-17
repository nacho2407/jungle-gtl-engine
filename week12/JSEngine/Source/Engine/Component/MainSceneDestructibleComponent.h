#pragma once

#include "Component/ActorComponent.h"

class AMainSceneDestructibleActor;

UCLASS()
class UMainSceneDestructibleComponent : public UActorComponent
{
public:
	GENERATED_BODY(UMainSceneDestructibleComponent, UActorComponent)

	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;

protected:
	void TickComponent(float DeltaTime) override;

private:
	float GetRealDeltaTime(float DeltaTime) const;
	bool StartSlice();

	UPROPERTY(DisplayName = "Auto Start")
	bool bAutoStart = true;

	UPROPERTY(DisplayName = "Slice Duration", Min = 0.05f, Max = 10.0f, Speed = 0.05f)
	float SliceDuration = 1.0f;

	UPROPERTY(DisplayName = "Slice Speed", Min = 0.0f, Max = 10.0f, Speed = 0.05f)
	float SliceSpeed = 1.2f;

	UPROPERTY(DisplayName = "Patrol Amplitude", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float PatrolAmplitude = 0.18f;

	UPROPERTY(DisplayName = "Patrol Speed", Min = 0.0f, Max = 20.0f, Speed = 0.05f)
	float PatrolSpeed = 1.15f;

	UPROPERTY(DisplayName = "Slice Count", Min = 1.0f, Max = 12.0f, Speed = 1.0f)
	int32 SliceCount = 5;

	UPROPERTY(DisplayName = "Presentation Trigger", Min = 0.0f, Max = 1.0f, Speed = 0.01f, Animatable)
	float PresentationTrigger = 0.0f;

	bool bPresentationTriggerConsumed = false;
	bool bSlicePending = false;
	bool bSliced = false;
	float Elapsed = 0.0f;
	float PatrolElapsed = 0.0f;
	TArray<AMainSceneDestructibleActor*> Fragments;
	TArray<FVector> FragmentStartLocations;
	TArray<FVector> FragmentTargetLocations;
};
