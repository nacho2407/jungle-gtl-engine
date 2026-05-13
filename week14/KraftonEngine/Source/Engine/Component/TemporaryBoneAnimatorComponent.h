#pragma once

#include "Component/ActorComponent.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

class USkeletalMesh;
class USkeletalMeshComponent;

// Temporary feature flag:
// kept intentionally so this experimental editor-only helper is easy to find
// and remove later by search.
#define JUNGLE_ENABLE_TEMP_BONE_ANIMATOR_COMPONENT 1

#if JUNGLE_ENABLE_TEMP_BONE_ANIMATOR_COMPONENT

class UTemporaryBoneAnimatorComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UTemporaryBoneAnimatorComponent, UActorComponent)

	UTemporaryBoneAnimatorComponent() = default;
	~UTemporaryBoneAnimatorComponent() override = default;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void ResolveTargetMeshComponent();
	void RefreshTargetBone();
	int32 FindBoneIndexByName(const USkeletalMesh* SkeletalMesh) const;
	void CaptureBasePose();
	void ApplyAnimatedBonePose(float DeltaTime);

private:
	FString TargetBoneName;
	FRotator RotationAmplitude = FRotator(0.0f, 20.0f, 0.0f);
	FRotator RotationFrequency = FRotator(1.0f, 1.0f, 1.0f);
	FRotator RotationPhase = FRotator(0.0f, 0.0f, 0.0f);
	FRotator RotationOffset = FRotator(0.0f, 0.0f, 0.0f);
	bool bEnabled = true;

	USkeletalMeshComponent* TargetMeshComponent = nullptr;
	USkeletalMesh* CachedSkeletalMesh = nullptr;
	int32 CachedBoneIndex = -1;
	FString CachedBoneName;
	FTransform BaseBoneLocalTransform;
	bool bHasCapturedBasePose = false;
	float ElapsedTime = 0.0f;
};

#endif
