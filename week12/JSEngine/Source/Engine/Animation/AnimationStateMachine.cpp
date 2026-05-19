#include "AnimationStateMachine.h"

#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "Asset/SkeletalMesh.h"
#include "Component/SkeletalMeshComponent.h"

#include <cmath>
#include <algorithm>

#include "Core/ResourceManager.h"

void FAnimSequencePoseSource::Update(float DeltaTime)
{
	if (!Sequence)
	{
		return;
	}

	PreviousTime = CurrentTime;
	const float PlayDeltaTime = DeltaTime * PlayRate;
	CurrentTime += PlayDeltaTime;

	bool bLooped = false;
	const bool bReverse = PlayDeltaTime < 0.0f;

	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f)
	{
		CurrentTime = 0.0f;
		PreviousTime = 0.0f;
		bFinished = true;
		return;
	}

	if (bLoop)
	{
		bFinished = false;
		if (!bReverse)
		{
			if (CurrentTime > Length)
			{
				CurrentTime = std::fmod(CurrentTime, Length);
				bLooped = true;
			}
		}
		else
		{
			if (CurrentTime < 0.0f)
			{
				CurrentTime = std::fmod(CurrentTime, Length);
				if (CurrentTime < 0.0f)
				{
					CurrentTime += Length;
				}
				bLooped = true;
			}
		}
	}
	else
	{
		if (!bReverse)
		{
			if (CurrentTime >= Length)
			{
				CurrentTime = Length;
				bFinished = true;
			}
		}
		else
		{
			if (CurrentTime <= 0.0f)
			{
				CurrentTime = 0.0f;
				bFinished = true;
			}
		}
	}

	UAnimInstance::DispatchAnimNotifies(OwnerComponent, Sequence, PreviousTime, CurrentTime, bLooped, bReverse, PlayDeltaTime);
}

bool FAnimSequencePoseSource::EvaluatePose(FPoseContext& OutPose) const
{
	if (!Sequence)
	{
		return false;
	}

	OutPose.TrackToBoneMap.clear();

	USkeletalMesh* Mesh = OwnerComponent ? OwnerComponent->GetSkeletalMesh() : nullptr;
	if (Mesh)
	{
		const TArray<FBoneAnimationTrack>& Tracks = Sequence->GetBoneAnimationTracks();
		const TArray<FBoneInfo>& Bones = Mesh->GetBones();

		OutPose.TrackToBoneMap.resize(Tracks.size(), -1);

		TMap<FName, int32, FName::Hash> BoneNameToIndex;
		BoneNameToIndex.reserve(Bones.size());

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
		{
			BoneNameToIndex[FName(Bones[BoneIndex].Name)] = BoneIndex;
		}

		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Tracks.size()); ++TrackIndex)
		{
			auto It = BoneNameToIndex.find(Tracks[TrackIndex].Name);
			if (It != BoneNameToIndex.end())
			{
				OutPose.TrackToBoneMap[TrackIndex] = It->second;
			}
		}
	}

	return Sequence->GetAnimationPose(CurrentTime, OutPose);
}

void FAnimSequencePoseSource::ResetTime()
{
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
	bFinished = false;
}

void UAnimationStateMachine::Initialize(USkeletalMeshComponent* Owner)
{
	OwnerComponent = Owner;
	OwnerPawn = OwnerComponent ? Cast<APawn>(OwnerComponent->GetOwner()) : nullptr;
}

void UAnimationStateMachine::AddState(FName StateName, UAnimSequenceBase* Sequence, float PlayRate, bool bLoop, bool bAutoAdvanceOnEnd)
{
	const bool bIsNewState = !States.contains(StateName);

	FAnimStateNode NewState;
	NewState.Name = StateName;
	NewState.PoseSource = std::make_shared<FAnimSequencePoseSource>(OwnerComponent, Sequence, PlayRate, bLoop);
	NewState.bAutoAdvanceOnEnd = bAutoAdvanceOnEnd;
	States[StateName] = NewState;

	if (bIsNewState)
	{
		StateOrder.push_back(StateName);
	}
}

void UAnimationStateMachine::AddTransition(FName FromState, FName ToState, float BlendTime, FAnimTransitionCondition Condition, int32 Priority)
{
	if (!States.contains(FromState) || !States.contains(ToState))
	{
		return;
	}

	FAnimTransition Transition;
	Transition.ToState = ToState;
	Transition.BlendTime = std::max(0.0f, BlendTime);
	Transition.Priority = Priority;
	Transition.Condition = Condition;

	States[FromState].Transitions.push_back(Transition);
}

void UAnimationStateMachine::ClearTransitions()
{
	for (auto& Pair : States)
	{
		Pair.second.Transitions.clear();
	}
}

void UAnimationStateMachine::SetEntryState(FName StateName)
{
	if (States.contains(StateName))
	{
		CurrentState = StateName;
		bBlending = false;
		States[CurrentState].PoseSource->ResetTime();
	}
}

void UAnimationStateMachine::SetState(FName NewState, float BlendTime)
{
	if (CurrentState == NewState || !States.contains(NewState))
	{
		return;
	}

	NextState = NewState;
	BlendElapsed = 0.0f;
	BlendDuration = std::max(0.0f, BlendTime);

	States[NextState].PoseSource->ResetTime();

	if (BlendDuration > 0.0f && States.contains(CurrentState))
	{
		bBlending = true;
	}
	else
	{
		CurrentState = NextState;
		bBlending = false;
	}
}

bool UAnimationStateMachine::TryStartTransitionFromCurrentState()
{
	if (!States.contains(CurrentState))
	{
		return false;
	}

	FAnimStateNode& CurrentNode = States[CurrentState];
	if (!CurrentNode.PoseSource)
	{
		return false;
	}

	const bool bWaitForEnd = CurrentNode.bAutoAdvanceOnEnd && !CurrentNode.PoseSource->IsLooping();
	if (bWaitForEnd && !CurrentNode.PoseSource->IsFinished())
	{
		return false;
	}

	const FAnimTransition* BestTransition = nullptr;
	for (const FAnimTransition& Transition : CurrentNode.Transitions)
	{
		if (!Transition.Condition || !Transition.Condition() || !States.contains(Transition.ToState))
		{
			continue;
		}

		if (!BestTransition || Transition.Priority > BestTransition->Priority)
		{
			BestTransition = &Transition;
		}
	}

	if (!BestTransition)
	{
		return false;
	}

	NextState = BestTransition->ToState;
	BlendDuration = std::max(0.0f, BestTransition->BlendTime);
	BlendElapsed = 0.0f;

	States[NextState].PoseSource->ResetTime();

	if (BlendDuration > 0.0f)
	{
		bBlending = true;
	}
	else
	{
		CurrentState = NextState;
		bBlending = false;
	}
	return true;
}

void UAnimationStateMachine::Update(float DeltaTime)
{
	if (!States.contains(CurrentState)) return;

	States[CurrentState].PoseSource->Update(DeltaTime);

	if (bBlending)
	{
		if (States.contains(NextState))
		{
			States[NextState].PoseSource->Update(DeltaTime);
		}

		BlendElapsed += DeltaTime;
		if (BlendElapsed >= BlendDuration)
		{
			CurrentState = NextState;
			bBlending = false;
		}
	}
	else
	{
		TryStartTransitionFromCurrentState();
	}
}

bool UAnimationStateMachine::EvaluatePose(FPoseContext& OutPose) const
{
	if (!States.contains(CurrentState)) return false;

	const FAnimStateNode& CurrentNode = States.at(CurrentState);
	if (!CurrentNode.PoseSource) return false;

	if (!bBlending || !States.contains(NextState))
	{
		return CurrentNode.PoseSource->EvaluatePose(OutPose);
	}

	const FAnimStateNode& NextNode = States.at(NextState);
	if (!NextNode.PoseSource)
	{
		return CurrentNode.PoseSource->EvaluatePose(OutPose);
	}

	FPoseContext CurrentPose = OutPose;
	FPoseContext NextPose = OutPose;

	bool bHasCurrent = CurrentNode.PoseSource->EvaluatePose(CurrentPose);
	bool bHasNext = NextNode.PoseSource->EvaluatePose(NextPose);

	if (bHasCurrent && bHasNext)
	{
		const float Alpha = BlendDuration > 0.0f
			? std::clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f)
			: 1.0f;
		return FAnimationRuntime::BlendTwoPosesTogether(CurrentPose, NextPose, Alpha, OutPose);
	}
	else if (bHasCurrent)
	{
		OutPose = CurrentPose;
		return true;
	}

	return false;
}

void UAnimationStateMachine::AddStateByName(const FString& StateName, UAnimSequenceBase* Sequence)
{
	AddState(FName(StateName.c_str()), Sequence);
}

void UAnimationStateMachine::AddStateFromPath(const FString& StateName, const FString& AnimPath)
{
	AddStateFromPathWithPlayback(StateName, AnimPath, 1.0f, true, true);
}

void UAnimationStateMachine::AddStateByNameWithPlayback(const FString& StateName, UAnimSequenceBase* Sequence, float PlayRate, bool bLoop, bool bAutoAdvanceOnEnd)
{
	AddState(FName(StateName.c_str()), Sequence, PlayRate, bLoop, bAutoAdvanceOnEnd);
}

void UAnimationStateMachine::AddStateFromPathWithPlayback(const FString& StateName, const FString& AnimPath, float PlayRate, bool bLoop, bool bAutoAdvanceOnEnd)
{
	UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(
		FResourceManager::Get().LoadAnimSequence(AnimPath));

	if (Sequence)
	{
		AddState(FName(StateName.c_str()), Sequence, PlayRate, bLoop, bAutoAdvanceOnEnd);
	}
}

void UAnimationStateMachine::SetEntryStateByName(const FString& StateName)
{
	SetEntryState(FName(StateName.c_str()));
}

void UAnimationStateMachine::SetStateByName(const FString& StateName, float BlendTime)
{
	SetState(FName(StateName.c_str()), BlendTime);
}

FString UAnimationStateMachine::GetCurrentStateName() const
{
	return CurrentState.ToString();
}

FString UAnimationStateMachine::GetNextStateName() const
{
	return NextState.ToString();
}

TArray<FString> UAnimationStateMachine::GetStateNames() const
{
	TArray<FString> Result;
	Result.reserve(StateOrder.size());

	for (const FName& StateName : StateOrder)
	{
		if (States.contains(StateName))
		{
			Result.push_back(StateName.ToString());
		}
	}

	return Result;
}

float UAnimationStateMachine::GetBlendAlpha() const
{
	if (!bBlending)
	{
		return 0.0f;
	}

	if (BlendDuration <= 0.0f)
	{
		return 1.0f;
	}

	return std::clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f);
}
