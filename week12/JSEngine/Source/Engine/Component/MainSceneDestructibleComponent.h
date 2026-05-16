#pragma once

#include "Component/ActorComponent.h"

class AMainSceneDestructibleActor;

UCLASS()
class UMainSceneDestructibleComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UMainSceneDestructibleComponent, UActorComponent)

	void BeginPlay() override;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

protected:
	void TickComponent(float DeltaTime) override;

private:
	float GetRealDeltaTime(float DeltaTime) const;
	bool StartSlice();

	bool bAutoStart = true;
	float SliceDuration = 1.0f;
	float SliceSpeed = 1.2f;
	float PatrolAmplitude = 0.18f;
	float PatrolSpeed = 1.15f;
	int32 SliceCount = 5;
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
