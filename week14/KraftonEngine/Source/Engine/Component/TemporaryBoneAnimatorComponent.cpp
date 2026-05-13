#include "TemporaryBoneAnimatorComponent.h"

#if JUNGLE_ENABLE_TEMP_BONE_ANIMATOR_COMPONENT

#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>

IMPLEMENT_CLASS(UTemporaryBoneAnimatorComponent, UActorComponent)

void UTemporaryBoneAnimatorComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << TargetBoneName;
	Ar << RotationAmplitude.Pitch;
	Ar << RotationAmplitude.Yaw;
	Ar << RotationAmplitude.Roll;
	Ar << RotationFrequency.Pitch;
	Ar << RotationFrequency.Yaw;
	Ar << RotationFrequency.Roll;
	Ar << RotationPhase.Pitch;
	Ar << RotationPhase.Yaw;
	Ar << RotationPhase.Roll;
	Ar << RotationOffset.Pitch;
	Ar << RotationOffset.Yaw;
	Ar << RotationOffset.Roll;
	Ar << bEnabled;
}

void UTemporaryBoneAnimatorComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	OutProps.push_back({ "Target Bone Name", EPropertyType::String, "Temp Bone Animator", &TargetBoneName });
	OutProps.push_back({ "Enabled", EPropertyType::Bool, "Temp Bone Animator", &bEnabled });
	OutProps.push_back({ "Rotation Amplitude", EPropertyType::Rotator, "Temp Bone Animator", &RotationAmplitude, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Rotation Frequency", EPropertyType::Rotator, "Temp Bone Animator", &RotationFrequency, 0.0f, 0.0f, 0.01f });
	OutProps.push_back({ "Rotation Phase", EPropertyType::Rotator, "Temp Bone Animator", &RotationPhase, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Rotation Offset", EPropertyType::Rotator, "Temp Bone Animator", &RotationOffset, 0.0f, 0.0f, 0.1f });
}

void UTemporaryBoneAnimatorComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Target Bone Name") == 0)
	{
		CachedBoneName.clear();
		CachedBoneIndex = -1;
		bHasCapturedBasePose = false;
	}
}

void UTemporaryBoneAnimatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!bEnabled)
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->bTickInEditor = true;
	}

	ResolveTargetMeshComponent();
	RefreshTargetBone();

	if (!TargetMeshComponent || CachedBoneIndex < 0)
	{
		return;
	}

	ApplyAnimatedBonePose(DeltaTime);
}

void UTemporaryBoneAnimatorComponent::ResolveTargetMeshComponent()
{
	if (TargetMeshComponent && TargetMeshComponent->GetOwner() == GetOwner())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	TargetMeshComponent = OwnerActor ? OwnerActor->GetComponentByClass<USkeletalMeshComponent>() : nullptr;
	CachedSkeletalMesh = nullptr;
	CachedBoneIndex = -1;
	bHasCapturedBasePose = false;
}

void UTemporaryBoneAnimatorComponent::RefreshTargetBone()
{
	if (!TargetMeshComponent)
	{
		CachedSkeletalMesh = nullptr;
		CachedBoneIndex = -1;
		bHasCapturedBasePose = false;
		return;
	}

	USkeletalMesh* SkeletalMesh = TargetMeshComponent->GetSkeletalMesh();
	if (CachedSkeletalMesh != SkeletalMesh || CachedBoneName != TargetBoneName)
	{
		CachedSkeletalMesh = SkeletalMesh;
		CachedBoneName = TargetBoneName;
		CachedBoneIndex = FindBoneIndexByName(SkeletalMesh);
		bHasCapturedBasePose = false;
	}

	if (CachedBoneIndex >= 0 && !bHasCapturedBasePose)
	{
		CaptureBasePose();
	}
}

int32 UTemporaryBoneAnimatorComponent::FindBoneIndexByName(const USkeletalMesh* SkeletalMesh) const
{
	if (!SkeletalMesh || TargetBoneName.empty())
	{
		return -1;
	}

	const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset)
	{
		return -1;
	}

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
	{
		if (Asset->Bones[BoneIndex].Name == TargetBoneName)
		{
			return BoneIndex;
		}
	}

	return -1;
}

void UTemporaryBoneAnimatorComponent::CaptureBasePose()
{
	if (!TargetMeshComponent || CachedBoneIndex < 0)
	{
		bHasCapturedBasePose = false;
		return;
	}

	BaseBoneLocalTransform = TargetMeshComponent->GetBoneLocalTransformByIndex(CachedBoneIndex);
	bHasCapturedBasePose = true;
	ElapsedTime = 0.0f;
}

void UTemporaryBoneAnimatorComponent::ApplyAnimatedBonePose(float DeltaTime)
{
	if (!bHasCapturedBasePose)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	const auto EvalAxis = [&](float Amplitude, float Frequency, float PhaseDegrees, float OffsetDegrees) -> float
	{
		return Amplitude * std::sinf((2.0f * FMath::Pi * Frequency * ElapsedTime) + (PhaseDegrees * DEG_TO_RAD)) + OffsetDegrees;
	};

	const FRotator AnimatedRotation(
		BaseBoneLocalTransform.GetRotator().Pitch + EvalAxis(RotationAmplitude.Pitch, RotationFrequency.Pitch, RotationPhase.Pitch, RotationOffset.Pitch),
		BaseBoneLocalTransform.GetRotator().Yaw + EvalAxis(RotationAmplitude.Yaw, RotationFrequency.Yaw, RotationPhase.Yaw, RotationOffset.Yaw),
		BaseBoneLocalTransform.GetRotator().Roll + EvalAxis(RotationAmplitude.Roll, RotationFrequency.Roll, RotationPhase.Roll, RotationOffset.Roll));

	FTransform AnimatedTransform = BaseBoneLocalTransform;
	AnimatedTransform.SetRotation(AnimatedRotation);

	TargetMeshComponent->SetBoneLocalTransformByIndex(CachedBoneIndex, AnimatedTransform);
}

#endif
