#include "AnimationStateMachine.h"
#include "AnimState.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "PoseContext.h"

DEFINE_CLASS(UAnimationStateMachine, UObject)

void UAnimationStateMachine::RegisterState(UAnimState* State)
{
	if (!State) return;

	// 같은 이름 재등록 → 교체.
	for (UAnimState*& Existing : States)
	{
		if (Existing && Existing->StateName == State->StateName)
		{
			Existing = State;
			return;
		}
	}
	States.push_back(State);

	// 시작 상태 미지정이면 첫 등록을 자동 지정.
	if (CurrentStateName == FName::None)
	{
		CurrentStateName = State->StateName;
		CurrentState     = State;
	}
}

void UAnimationStateMachine::RegisterTransition(const FStateTransition& T)
{
	Transitions.push_back(T);
}

void UAnimationStateMachine::SetInitialState(FName StateName)
{
	if (UAnimState* S = FindState(StateName))
	{
		CurrentStateName = StateName;
		CurrentState     = S;
	}
}

void UAnimationStateMachine::Tick(UAnimInstance* Owner, float DeltaSeconds)
{
	if (!CurrentState) return;

	// 1) 전이 평가 — From 이 현재 상태이거나 AnyState(None) 인 것 중 첫 매치.
	for (const FStateTransition& T : Transitions)
	{
		const bool bMatchesFrom = (T.From == FName::None) || (T.From == CurrentStateName);
		if (!bMatchesFrom) continue;
		if (T.To == CurrentStateName) continue; // self-transition 방지
		if (T.Condition && T.Condition(Owner))
		{
			BeginBlend(Owner, T.To, T.BlendTime);
			break;
		}
	}

	// 2) 현재 상태 시간 진행.
	CurrentState->Tick(Owner, DeltaSeconds);

	// 3) 블렌딩 중이면 FromState 도 시간 진행 + 블렌드 알파 증가.
	if (FromState)
	{
		FromState->Tick(Owner, DeltaSeconds);
		if (BlendDuration > 0.0f)
		{
			BlendAlpha += DeltaSeconds / BlendDuration;
			if (BlendAlpha >= 1.0f)
			{
				FinishBlend(Owner);
			}
		}
		else
		{
			FinishBlend(Owner);
		}
	}
}

void UAnimationStateMachine::Evaluate(UAnimInstance* Owner, FPoseContext& Output)
{
	if (!CurrentState)
	{
		Output.ResetToRefPose();
		return;
	}

	if (FromState)
	{
		// 두 상태 평가 후 BlendTwoPosesTogether.
		FPoseContext FromPose;
		FromPose.SkeletalMesh = Output.SkeletalMesh;
		FromPose.Pose.resize(Output.Pose.size());

		FPoseContext ToPose;
		ToPose.SkeletalMesh = Output.SkeletalMesh;
		ToPose.Pose.resize(Output.Pose.size());

		FromState->Evaluate(Owner, FromPose);
		CurrentState->Evaluate(Owner, ToPose);

		FAnimationRuntime::BlendTwoPosesTogether(FromPose, ToPose, BlendAlpha, Output);
	}
	else
	{
		CurrentState->Evaluate(Owner, Output);
	}
}

void UAnimationStateMachine::RequestTransition(FName To, float BlendDuration_)
{
	BeginBlend(nullptr, To, BlendDuration_);
}

UAnimState* UAnimationStateMachine::FindState(FName Name) const
{
	for (UAnimState* S : States)
	{
		if (S && S->StateName == Name) return S;
	}
	return nullptr;
}

void UAnimationStateMachine::EnterState(UAnimInstance* Owner, FName NewState)
{
	UAnimState* Target = FindState(NewState);
	if (!Target) return;

	if (CurrentState) CurrentState->OnExit(Owner);
	CurrentState     = Target;
	CurrentStateName = NewState;
	CurrentState->OnEnter(Owner);
}

void UAnimationStateMachine::BeginBlend(UAnimInstance* Owner, FName NewState, float Duration)
{
	UAnimState* Target = FindState(NewState);
	if (!Target || Target == CurrentState) return;

	// 이전 블렌드가 끝나기 전 새 전이 — 진행중 From 은 버리고 현재 상태를 새 From 으로.
	FromState        = CurrentState;
	BlendAlpha       = 0.0f;
	BlendDuration    = Duration;

	CurrentState     = Target;
	CurrentStateName = NewState;
	CurrentState->OnEnter(Owner);

	if (Duration <= 0.0f)
	{
		FinishBlend(Owner);
	}
}

void UAnimationStateMachine::FinishBlend(UAnimInstance* Owner)
{
	if (FromState) FromState->OnExit(Owner);
	FromState     = nullptr;
	BlendAlpha    = 1.0f;
	BlendDuration = 0.0f;
}
