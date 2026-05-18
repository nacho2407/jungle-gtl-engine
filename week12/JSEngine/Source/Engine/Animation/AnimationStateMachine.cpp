#include "AnimationStateMachine.h"

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

    CurrentTime += DeltaTime;

    const float Length = Sequence->GetPlayLength();
    if (Length > 0.0f)
    {
        CurrentTime = std::fmod(CurrentTime, Length);
    }
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
}

void UAnimationStateMachine::Initialize(USkeletalMeshComponent* Owner)
{
    OwnerComponent = Owner;
    OwnerPawn = OwnerComponent ? Cast<APawn>(OwnerComponent->GetOwner()) : nullptr;
}

void UAnimationStateMachine::AddState(FName StateName, UAnimSequenceBase* Sequence)
{
	FAnimStateNode NewState;
	NewState.Name = StateName;
    NewState.PoseSource = std::make_shared<FAnimSequencePoseSource>(OwnerComponent, Sequence);
	States[StateName] = NewState;
}

void UAnimationStateMachine::AddTransition(FName FromState, FName ToState, float BlendTime, FAnimTransitionCondition Condition)
{
    if (!States.contains(FromState) || !States.contains(ToState))
    {
        return;
    }

    FAnimTransition Transition;
    Transition.ToState = ToState;
    Transition.BlendTime = std::max(0.0f, BlendTime);
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
        for (const FAnimTransition& Transition : States[CurrentState].Transitions)
        {
            if (Transition.Condition && Transition.Condition() && States.contains(Transition.ToState))
            {
                NextState = Transition.ToState;
                BlendDuration = std::max(0.0f, Transition.BlendTime);
                BlendElapsed = 0.0f;

                if (States.contains(NextState))
                {
                    States[NextState].PoseSource->ResetTime();
                }

                if (BlendDuration > 0.0f)
                {
                    bBlending = true;
                }
                else
                {
                    CurrentState = NextState;
                    bBlending = false;
                }
                break;
            }
        }
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
    UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(
        FResourceManager::Get().LoadAnimSequence(AnimPath));

    if (Sequence)
    {
        AddState(FName(StateName.c_str()), Sequence);
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
    Result.reserve(States.size());

    for (const auto& Pair : States)
    {
        Result.push_back(Pair.first.ToString());
    }

    return Result;
}

TArray<FAnimTransitionDebugInfo> UAnimationStateMachine::GetTransitionDebugInfos() const
{
    TArray<FAnimTransitionDebugInfo> Result;

    for (const auto& Pair : States)
    {
        const FString FromState = Pair.first.ToString();
        const FAnimStateNode& State = Pair.second;

        for (const FAnimTransition& Transition : State.Transitions)
        {
            FAnimTransitionDebugInfo Info;
            Info.FromState = FromState;
            Info.ToState = Transition.ToState.ToString();
            Info.BlendTime = Transition.BlendTime;
            Result.push_back(Info);
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
