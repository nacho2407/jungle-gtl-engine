#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Core/Logging/SkinningStats.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Core/ResourceManager.h"

#include <cstring>

DEFINE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
REGISTER_FACTORY(USkeletalMeshComponent)
void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);

	if (Ar.IsLoading() && !AnimationAssetPath.empty())
	{
		SetAnimation(FResourceManager::Get().LoadAnimSequence(AnimationAssetPath));
	}
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	USkinnedMeshComponent::PostEditProperty(PropertyName);

	if (PropertyName && std::strcmp(PropertyName, "AnimationAssetPath") == 0)
	{
		SetAnimation(AnimationAssetPath.empty() ? nullptr : FResourceManager::Get().LoadAnimSequence(AnimationAssetPath));
	}
	else if (PropertyName && std::strcmp(PropertyName, "AnimationMode") == 0)
	{
		SetAnimationMode(AnimationMode);
	}
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	USkinnedMeshComponent::TickComponent(DeltaTime);

	if (AnimInstance)
	{
		SKINNING_SCOPE_MS(&FSkinningStats::AddCPUAnimationUpdate);
		AnimInstance->NativeUpdateAnimation(DeltaTime);

		FPoseContext PoseContext;
		const int32 BoneCount = static_cast<int32>(CurrentLocalPose.size());
		PoseContext.LocalPose = CurrentLocalPose;
		PoseContext.BindPose.resize(BoneCount, FMatrix::Identity);

		if (SkeletalMesh)
		{
			for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
			{
				PoseContext.BindPose[BoneIndex] = SkeletalMesh->GetLocalBindTransform(BoneIndex);
			}
		}

		if (AnimInstance->EvaluatePose(PoseContext))
		{
			ApplyAnimationPose(PoseContext);
		}
	}

	// Pose가 바뀐 경우에만 실제 CPU skinning이 수행(dirty flag 이용)
	EnsureSkinningUpdated();
}

void USkeletalMeshComponent::ApplyAnimationPose(const FPoseContext& PoseContext)
{
	SetCurrentLocalPose(PoseContext.LocalPose);
}

void USkeletalMeshComponent::ResetToBindPose()
{
	InitializePoseFromBindPose();
	MarkSkinningDirty();
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform)
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
	{
		return;
	}

	CurrentLocalPose[BoneIndex] = NewLocalTransform;
	UpdateCurrentGlobalPose();
	MarkSkinningDirty();
}

const FMatrix& USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	// fallback은 identity
	static const FMatrix Identity = FMatrix::Identity;

	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
	{
		return Identity;
	}

	return CurrentLocalPose[BoneIndex];
}

FMatrix USkeletalMeshComponent::GetBoneGlobalTransform(int32 BoneIndex) const
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
	{
		return FMatrix::Identity;
	}

	return CurrentGlobalPose[BoneIndex] * GetWorldMatrix();
}

void USkeletalMeshComponent::SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform)
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentLocalPose.size()))
	{
		return;
	}

	if (!SkeletalMesh)
	{
		return;
	}

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
	if (BoneIndex >= static_cast<int32>(Bones.size()))
	{
		return;
	}

	int32 ParentIndex = Bones[BoneIndex].ParentIndex;

	FMatrix ParentGlobalTransform;
	if (ParentIndex >= 0)
	{
		ParentGlobalTransform = CurrentGlobalPose[ParentIndex] * GetWorldMatrix();
	}
	else
	{
		ParentGlobalTransform = GetWorldMatrix();
	}

	// Local = Global * ParentGlobal.Inverse
	FMatrix NewLocalTransform = NewGlobalTransform * ParentGlobalTransform.GetInverse();
	SetBoneLocalTransform(BoneIndex, NewLocalTransform);
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping)
{
	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SetAnimation(NewAnimToPlay);

	UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
	if (!SingleNode)
	{
		SingleNode = new UAnimSingleNodeInstance();
		SingleNode->Initialize(this);

		AnimInstance = SingleNode;
	}

	SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimToPlay));
	Play(bLooping);
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InAnimationMode)
{
	AnimationMode = InAnimationMode;
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimation)
{
	AnimationToPlay = NewAnimation;
	if (auto* SingleNode = dynamic_cast<UAnimSingleNodeInstance*>(AnimInstance))
	{
		SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimation));
	}
}

void USkeletalMeshComponent::Play(bool bInLooping)
{
	bLooping = bInLooping;
	bPlaying = AnimationToPlay != nullptr;

	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->Play(bInLooping);
	}
}

void USkeletalMeshComponent::Stop()
{
	bPlaying = false;
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->Stop();
	}
}

void USkeletalMeshComponent::Pause()
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->Pause();
	}
}

void USkeletalMeshComponent::SetPlayRate(float InPlayRate)
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->SetPlayRate(InPlayRate);
	}
}

void USkeletalMeshComponent::SetAnimationPosition(float InTime)
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->SetPosition(InTime);
	}
}

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyEvent& Notify)
{
	UE_LOG("[AnimNotify] %s triggered at %.3f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
	OnAnimNotifyDelegate.Broadcast(this, Notify);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnAnimNotify(this, Notify);
	}
}
