#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/StateMachineAnimInstance.h"
#include "Core/Logging/SkinningStats.h"
#include "Core/Paths.h"
#include "GameFramework/AActor.h"

#include <cstring>

namespace
{
	FString GetPersistentAnimationAssetPath(UAnimationAsset* Animation);
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);

	if (Ar.IsLoading())
	{
		SetAnimationMode(AnimationMode);
		bool bLoadedAnimInstance = false;
		if (AnimInstance && Ar.HasKey("AnimInstance"))
		{
			Ar.BeginObject("AnimInstance");
			AnimInstance->Serialize(Ar);
			Ar.EndObject();
			AnimationToPlay = Cast<UAnimSingleNodeInstance>(AnimInstance)
				? Cast<UAnimSingleNodeInstance>(AnimInstance)->GetAnimation()
				: nullptr;
			SyncAnimationAssetPathFromAnimation(AnimationToPlay);
			bLoadedAnimInstance = true;
		}
		if (!bLoadedAnimInstance && (!AnimationAssetPath.GetPath().empty() || AnimationMode == EAnimationMode::AnimationSingleNode))
		{
			ApplyAnimationFromAssetPath();
		}
	}
	else if (Ar.IsSaving())
	{
		if (AnimationMode == EAnimationMode::AnimationSingleNode)
		{
			EnsureSingleNodeInstance();
		}

		if (AnimInstance)
		{
			Ar.BeginObject("AnimInstance");
			AnimInstance->Serialize(Ar);
			Ar.EndObject();
		}
	}
}

void USkeletalMeshComponent::PostDuplicate(UObject* Original)
{
	USkinnedMeshComponent::PostDuplicate(Original);

	USkeletalMeshComponent* SourceComponent = Cast<USkeletalMeshComponent>(Original);
	if (!SourceComponent)
	{
		return;
	}

	AnimInstance = nullptr;
	AnimationAssetPath.SetPath(SourceComponent->AnimationAssetPath.GetPath());
	AnimationToPlay = SourceComponent->AnimationToPlay;
	AnimationMode = SourceComponent->AnimationMode;

	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
		if (UAnimSingleNodeInstance* SourceSingleNode = Cast<UAnimSingleNodeInstance>(SourceComponent->GetAnimInstance()))
		{
			SingleNode->CopyPropertiesFrom(SourceSingleNode);
			SingleNode->PostEditChangeProperty({ "AnimationAssetPath", EPropertyChangeType::ValueSet });
		}
	}
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	USkinnedMeshComponent::PostEditProperty(PropertyName);

	if (PropertyName && std::strcmp(PropertyName, "AnimationMode") == 0)
	{
		SetAnimationMode(AnimationMode);
		if (AnimationMode == EAnimationMode::AnimationSingleNode && !AnimationAssetPath.GetPath().empty())
		{
			ApplyAnimationFromAssetPath();
		}
	}
	else if (PropertyName && std::strcmp(PropertyName, "AnimationAssetPath") == 0)
	{
		ApplyAnimationFromAssetPath();
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
			//4.Skinning Phase
			ApplyAnimationPose(PoseContext);
		}
	}

	// Pose가 바뀐 경우에만 실제 CPU skinning이 수행(dirty flag 이용)
	EnsureSkinningUpdated();
}

//4.Skinning Phase
//Local Pose를 컴포넌트에 적용, skinning update
void USkeletalMeshComponent::ApplyAnimationPose(const FPoseContext& PoseContext)
{
	SetCurrentLocalPose(PoseContext.LocalPose);
}

UAnimationStateMachine* USkeletalMeshComponent::CreateAnimationStateMachine()
{
	if (UAnimationStateMachine* ExistingStateMachine = GetAnimationStateMachine())
	{
		return ExistingStateMachine;
	}

	UAnimationStateMachine* NewStateMachine = UObjectManager::Get().CreateObject<UAnimationStateMachine>();
	NewStateMachine->Initialize(this);
	SetAnimationStateMachine(NewStateMachine);
	return NewStateMachine;
}

void USkeletalMeshComponent::SetAnimationStateMachine(UAnimationStateMachine* InStateMachine)
{
	if (!InStateMachine)
	{
		return;
	}

	InStateMachine->Initialize(this);

	UStateMachineAnimInstance* Instance = UObjectManager::Get().CreateObject<UStateMachineAnimInstance>();

	Instance->Initialize(this);
	Instance->SetStateMachine(InStateMachine);

	AnimInstance = Instance;
	AnimationMode = EAnimationMode::AnimationCustomMode;
}

UAnimationStateMachine* USkeletalMeshComponent::GetAnimationStateMachine() const
{
	if (auto* StateMachineInstance = Cast<UStateMachineAnimInstance>(AnimInstance))
	{
		return StateMachineInstance->GetStateMachine();
	}

	return nullptr;
}

void USkeletalMeshComponent::SetAnimStateByName(const FString& StateName, float BlendTime)
{
	if (UAnimationStateMachine* StateMachine = GetAnimationStateMachine())
	{
		StateMachine->SetStateByName(StateName, BlendTime);
	}
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

	UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
	SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimToPlay));
	Play(bLooping);
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InAnimationMode)
{
	AnimationMode = InAnimationMode;

	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
		if (AnimationToPlay)
		{
			SingleNode->SetAnimation(Cast<UAnimSequenceBase>(AnimationToPlay));
			SyncAnimationAssetPathFromAnimation(AnimationToPlay);
		}
		else if (!AnimationAssetPath.GetPath().empty())
		{
			SingleNode->SetAnimationAssetPath(AnimationAssetPath.GetPath());
			AnimationToPlay = SingleNode->GetAnimation();
		}
	}
}

void USkeletalMeshComponent::SetAnimation(UAnimationAsset* NewAnimation)
{
	AnimationToPlay = NewAnimation;
	SyncAnimationAssetPathFromAnimation(NewAnimation);

	if (!AnimationToPlay)
	{
		ResetToBindPose();
	}

	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
		SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimation));
	}
}

UAnimSingleNodeInstance* USkeletalMeshComponent::EnsureSingleNodeInstance()
{
	UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance);
	if (!SingleNode)
	{
		SingleNode = UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>();
		SingleNode->Initialize(this);
		AnimInstance = SingleNode;
	}
	return SingleNode;
}

void USkeletalMeshComponent::ApplyAnimationFromAssetPath()
{
	const FString RequestedPath = AnimationAssetPath.GetPath();
	if (RequestedPath.empty())
	{
		SetAnimation(nullptr);
		return;
	}

	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
	SingleNode->SetAnimationAssetPath(RequestedPath);
	AnimationToPlay = SingleNode->GetAnimation();
	if (!AnimationToPlay)
	{
		AnimationAssetPath.SetPath(RequestedPath);
		ResetToBindPose();
	}
}

void USkeletalMeshComponent::SyncAnimationAssetPathFromAnimation(UAnimationAsset* Animation)
{
	if (!Animation)
	{
		AnimationAssetPath.SetPath("");
		return;
	}

	const FString PersistentPath = GetPersistentAnimationAssetPath(Animation);
	if (!PersistentPath.empty())
	{
		AnimationAssetPath.SetPath(PersistentPath);
	}
}

void USkeletalMeshComponent::Play(bool bInLooping)
{
	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		UAnimSingleNodeInstance* SingleNode = EnsureSingleNodeInstance();
		if (!SingleNode->GetAnimation() && !AnimationAssetPath.GetPath().empty())
		{
			SingleNode->SetAnimationAssetPath(AnimationAssetPath.GetPath());
			AnimationToPlay = SingleNode->GetAnimation();
		}
		SingleNode->Play(bInLooping);
	}
}

void USkeletalMeshComponent::Stop()
{
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

float USkeletalMeshComponent::GetPlayRate() const
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		return SingleNode->GetPlayRate();
	}

	return 1.0f;
}

FString USkeletalMeshComponent::GetAnimationAssetPath() const
{
	if (!AnimationAssetPath.GetPath().empty())
	{
		return AnimationAssetPath.GetPath();
	}
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		return SingleNode->GetAnimationAssetPath();
	}

	return "";
}

bool USkeletalMeshComponent::IsPlaying() const
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		return SingleNode->IsPlaying();
	}

	return false;
}

bool USkeletalMeshComponent::IsLooping() const
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		return SingleNode->IsLooping();
	}

	return false;
}

void USkeletalMeshComponent::SetLooping(bool bInLooping)
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		SingleNode->SetLooping(bInLooping);
	}
}

namespace
{
	FString GetPersistentAnimationAssetPath(UAnimationAsset* Animation)
	{
		UAnimSequence* Sequence = Cast<UAnimSequence>(Animation);
		if (!Sequence)
		{
			return "";
		}

		if (!Sequence->GetAssetPath().empty())
		{
			return FPaths::Normalize(Sequence->GetAssetPath());
		}

		if (!Sequence->GetSourceFilePath().empty())
		{
			return FPaths::Normalize(Sequence->GetSourceFilePath());
		}

		return "";
	}
}

float USkeletalMeshComponent::GetAnimationPosition() const
{
	return AnimInstance ? AnimInstance->GetCurrentTime() : 0.0f;
}

float USkeletalMeshComponent::GetAnimationLength() const
{
	if (auto* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
	{
		return SingleNode->GetLength();
	}

	return 0.0f;
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
