#include "SkeletalMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimNotify.h"
#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationStateMachine.h"
#include "Animation/StateMachineAnimInstance.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/ResourceManager.h"
#include "Core/Logging/SkinningStats.h"
#include "Core/Paths.h"
#include "GameFramework/AActor.h"
#include "Object/Class.h"
#include "Object/Object.h"

#include <cstring>

namespace
{
	constexpr const char* AnimInstanceDataKey = "AnimInstanceData";
	constexpr const char* SerializedObjectTypeKey = "Type";
	constexpr bool bDispatchLegacyAnimNotifyCallbacks = false;

	FString GetPersistentAnimationAssetPath(UAnimationAsset* Animation);
	UAnimInstance* CreateAnimInstanceFromClassName(const FString& ClassName);

	UAnimNotify* ResolveAnimNotifyObject(const FAnimNotifyStateEvent& Notify)
	{
		return Notify.NotifyObject;
	}
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		if (AnimInstance)
		{
			Ar.BeginObject(AnimInstanceDataKey);
			AnimInstance->Serialize(Ar);
			Ar.EndObject();
		}
		return;
	}

	if (!Ar.IsLoading())
	{
		return;
	}

	if (Ar.HasKey(AnimInstanceDataKey))
	{
		Ar.BeginObject(AnimInstanceDataKey);

		FString AnimInstanceClassName;
		if (Ar.HasKey(SerializedObjectTypeKey))
		{
			Ar << SerializedObjectTypeKey << AnimInstanceClassName;
		}

		UAnimInstance* LoadedAnimInstance = CreateAnimInstanceFromClassName(AnimInstanceClassName);
		if (LoadedAnimInstance)
		{
			LoadedAnimInstance->Serialize(Ar);
			LoadedAnimInstance->Initialize(this);
			AnimInstance = LoadedAnimInstance;

			if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
			{
				const FString& LoadedAnimationPath = SingleNode->GetAnimationAssetPath();
				if (!LoadedAnimationPath.empty())
				{
					AnimationAssetPath.SetPath(LoadedAnimationPath);
				}
				AnimationToPlay = Cast<UAnimationAsset>(SingleNode->GetAnimation());
			}
			else if (UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
			{
				if (!AnimGraphAssetPath.IsNull())
				{
					UAnimGraphAsset* Graph = FResourceManager::Get().LoadAnimGraph(FPaths::Normalize(AnimGraphAssetPath.GetPath()));
					GraphInstance->SetGraphAsset(Graph);
				}
			}
		}

		Ar.EndObject();
		return;
	}

	SetAnimationMode(AnimationMode);

	if (!AnimationAssetPath.GetPath().empty() || AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		ApplyAnimationFromAssetPath();
	}

	if (AnimationMode == EAnimationMode::AnimationGraph)
	{
		ApplyAnimGraphFromAssetPath();
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
	AnimGraphAssetPath = SourceComponent->AnimGraphAssetPath;
	AnimationToPlay = SourceComponent->AnimationToPlay;
	AnimationMode = SourceComponent->AnimationMode;
	UAnimSingleNodeInstance* SourceSingleNode = Cast<UAnimSingleNodeInstance>(SourceComponent->AnimInstance);

	// SingleNodeInstance가 들고 있는 최신 path를 우선 사용하고,
	// 없으면 component의 AnimationAssetPath를 fallback으로 사용합니다.
	if (SourceSingleNode && !SourceSingleNode->GetAnimationAssetPath().empty())
	{
		AnimationAssetPath.SetPath(SourceSingleNode->GetAnimationAssetPath());
	}
	else
	{
		AnimationAssetPath.SetPath(SourceComponent->AnimationAssetPath.GetPath());
	}

	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		ApplyAnimationFromAssetPath();
		if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
		{
			SingleNode->CopyPlaybackSettingsFrom(SourceSingleNode);
		}
	}
	else if (AnimationMode == EAnimationMode::AnimationGraph)
	{
		ApplyAnimGraphFromAssetPath();
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
	else if (PropertyName && std::strcmp(PropertyName, "AnimGraphAssetPath") == 0)
	{
		if (!AnimGraphAssetPath.IsNull() && AnimationMode != EAnimationMode::AnimationGraph)
		{
			SetAnimationMode(EAnimationMode::AnimationGraph);
		}
		else
		{
			ApplyAnimGraphFromAssetPath();
		}
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
	AnimationMode = EAnimationMode::AnimationStateMachine;
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

void USkeletalMeshComponent::SetAnimGraph(UAnimGraphAsset* Graph)
{
	if (!Graph)
	{
		if (AnimationMode == EAnimationMode::AnimationGraph)
		{
			AnimInstance = nullptr;
			ResetToBindPose();
		}
		return;
	}

	UAnimGraphInstance* Instance = UObjectManager::Get().CreateObject<UAnimGraphInstance>();
	Instance->Initialize(this);
	Instance->SetGraphAsset(Graph);

	AnimInstance = Instance;
	AnimationMode = EAnimationMode::AnimationGraph;
}

void USkeletalMeshComponent::SetAnimGraphAssetPath(const FString& Path)
{
	AnimGraphAssetPath.SetPath(FPaths::Normalize(Path));
	SetAnimationMode(EAnimationMode::AnimationGraph);
}

void USkeletalMeshComponent::ApplyAnimGraphFromAssetPath()
{
	if (AnimGraphAssetPath.IsNull())
	{
		if (AnimationMode == EAnimationMode::AnimationGraph)
		{
			UE_LOG_WARNING("[SkeletalMeshComponent] AnimationGraph mode selected, but AnimGraphAssetPath is empty.");
			AnimInstance = nullptr;
			ResetToBindPose();
		}
		return;
	}

	const FString GraphPath = FPaths::Normalize(AnimGraphAssetPath.GetPath());
	AnimGraphAssetPath.SetPath(GraphPath);

	UAnimGraphAsset* Graph = FResourceManager::Get().LoadAnimGraph(GraphPath);
	if (!Graph)
	{
		UE_LOG_WARNING("[SkeletalMeshComponent] Failed to load anim graph: %s", GraphPath.c_str());
		if (AnimationMode == EAnimationMode::AnimationGraph)
		{
			AnimInstance = nullptr;
			ResetToBindPose();
		}
		return;
	}

	SetAnimGraph(Graph);
}

void USkeletalMeshComponent::SetAnimGraphFloatParameter(const FString& Name, float Value)
{
	if (UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		GraphInstance->SetFloatParameter(Name, Value);
	}
}

void USkeletalMeshComponent::SetAnimGraphBoolParameter(const FString& Name, bool Value)
{
	if (UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		GraphInstance->SetBoolParameter(Name, Value);
	}
}

void USkeletalMeshComponent::SetAnimGraphIntParameter(const FString& Name, int32 Value)
{
	if (UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		GraphInstance->SetIntParameter(Name, Value);
	}
}

float USkeletalMeshComponent::GetAnimGraphFloatParameter(const FString& Name) const
{
	if (const UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		return GraphInstance->GetFloatParameter(Name);
	}

	return 0.0f;
}

bool USkeletalMeshComponent::GetAnimGraphBoolParameter(const FString& Name) const
{
	if (const UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		return GraphInstance->GetBoolParameter(Name);
	}

	return false;
}

int32 USkeletalMeshComponent::GetAnimGraphIntParameter(const FString& Name) const
{
	if (const UAnimGraphInstance* GraphInstance = Cast<UAnimGraphInstance>(AnimInstance))
	{
		return GraphInstance->GetIntParameter(Name);
	}

	return 0;
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

	UAnimSingleNodeInstance* SingleNode = GetOrCreateSingleNodeInstance();
	SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimToPlay));
	Play(bLooping);
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InAnimationMode)
{
	AnimationMode = InAnimationMode;

	if (AnimationMode == EAnimationMode::AnimationSingleNode)
	{
		UAnimSingleNodeInstance* SingleNode = GetOrCreateSingleNodeInstance();
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
	else if (AnimationMode == EAnimationMode::AnimationGraph)
	{
		ApplyAnimGraphFromAssetPath();
	}
	else
	{
		AnimInstance = nullptr;
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
		UAnimSingleNodeInstance* SingleNode = GetOrCreateSingleNodeInstance();
		SingleNode->SetAnimation(Cast<UAnimSequenceBase>(NewAnimation));
	}
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
	return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetOrCreateSingleNodeInstance()
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

	// AnimationToPlay가 예전 포인터를 들고 있으면 SetAnimationMode 내부에서
	// SyncAnimationAssetPathFromAnimation()이 stale path로 덮어쓰므로, 미리 클리어합니다.
	AnimationToPlay = nullptr;

	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	UAnimSingleNodeInstance* SingleNode = GetOrCreateSingleNodeInstance();
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
		UAnimSingleNodeInstance* SingleNode = GetOrCreateSingleNodeInstance();
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

void USkeletalMeshComponent::HandleAnimNotify(const FAnimNotifyStateEvent& Notify)
{
	UE_LOG("[AnimNotify] %s triggered at %.3f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime);
	if (UAnimNotify* NotifyObject = ResolveAnimNotifyObject(Notify))
	{
		NotifyObject->Notify(this, Notify);
	}

	if (bDispatchLegacyAnimNotifyCallbacks)
	{
		OnAnimNotifyDelegate.Broadcast(this, Notify);

		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->OnAnimNotify(this, Notify);
		}
	}
}

void USkeletalMeshComponent::HandleAnimNotifyBegin(const FAnimNotifyStateEvent& Notify)
{
	UE_LOG("[AnimNotifyBegin] %s begin at %.3f duration %.3f", Notify.NotifyName.ToString().c_str(), Notify.TriggerTime, Notify.Duration);
	if (UAnimNotify* NotifyObject = ResolveAnimNotifyObject(Notify))
	{
		NotifyObject->NotifyBegin(this, Notify);
	}

	if (bDispatchLegacyAnimNotifyCallbacks)
	{
		OnAnimNotifyBeginDelegate.Broadcast(this, Notify);

		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->OnAnimNotifyBegin(this, Notify);
		}
	}
}

void USkeletalMeshComponent::HandleAnimNotifyTick(const FAnimNotifyStateEvent& Notify, float DeltaTime)
{
	if (UAnimNotify* NotifyObject = ResolveAnimNotifyObject(Notify))
	{
		NotifyObject->NotifyTick(this, Notify, DeltaTime);
	}

	if (bDispatchLegacyAnimNotifyCallbacks)
	{
		OnAnimNotifyTickDelegate.Broadcast(this, Notify, DeltaTime);

		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->OnAnimNotifyTick(this, Notify, DeltaTime);
		}
	}
}

void USkeletalMeshComponent::HandleAnimNotifyEnd(const FAnimNotifyStateEvent& Notify)
{
	UE_LOG("[AnimNotifyEnd] %s end at %.3f", Notify.NotifyName.ToString().c_str(), Notify.GetEndTime());
	if (UAnimNotify* NotifyObject = ResolveAnimNotifyObject(Notify))
	{
		NotifyObject->NotifyEnd(this, Notify);
	}

	if (bDispatchLegacyAnimNotifyCallbacks)
	{
		OnAnimNotifyEndDelegate.Broadcast(this, Notify);

		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->OnAnimNotifyEnd(this, Notify);
		}
	}
}

namespace
{
	UAnimInstance* CreateAnimInstanceFromClassName(const FString& ClassName)
	{
		UClass* Class = FReflectionRegistry::Get().FindClass(ClassName);
		if (!Class || !Class->IsChildOf(UAnimInstance::StaticClass()) || Class->HasAnyClassFlags(CF_Abstract))
		{
			return nullptr;
		}

		return Cast<UAnimInstance>(NewObject(Class));
	}
}
