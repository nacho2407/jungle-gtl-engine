#include "AnimationStateMachine.h"
#include "AnimState.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "PoseContext.h"
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

	// 4) Root motion delta 누적 — blend 중이면 weight lerp.
	//    From: weight (1 - BlendAlpha), Current: weight BlendAlpha. 합한 delta 를 instance 에 push.
	//    RootMotionFromMontagesOnly mode 일 때 FSM base 누적 skip (Montage 만 적용되도록).
	if (Owner && Owner->GetRootMotionMode() != ERootMotionMode::RootMotionFromMontagesOnly)
	{
		const FTransform& CurDelta = CurrentState->GetLastRootMotionDelta();
		if (FromState)
		{
			const FTransform& FromDelta = FromState->GetLastRootMotionDelta();
			const float wTo   = BlendAlpha;
			const float wFrom = 1.0f - BlendAlpha;
			FTransform Blended;
			Blended.Location = FromDelta.Location * wFrom + CurDelta.Location * wTo;
			Blended.Rotation = FQuat::Slerp(FromDelta.Rotation.GetNormalized(), CurDelta.Rotation.GetNormalized(), BlendAlpha).GetNormalized();
			Owner->AccumulateRootMotion(Blended);
		}
		else
		{
			Owner->AccumulateRootMotion(CurDelta);
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
		// ★ ResetToRefPose 필수 — Sequence->GetBonePose 는 트랙 있는 본만 덮어씀.
		//   ref pose 로 시작 안 하면 트랙 없는 본은 default FTransform(T=0,R=identity,S=1) 로 남고,
		//   상대편 정상 pose 와 lerp 되어 본들이 부모 기준 (0,0,0) 으로 끌려감 ("바닥에 꼬꾸라짐").
		FPoseContext FromPose;
		FromPose.SkeletalMesh = Output.SkeletalMesh;
		FromPose.ResetToRefPose();   // resize + bone[i].LocalMatrix 분해해서 채움

		FPoseContext ToPose;
		ToPose.SkeletalMesh = Output.SkeletalMesh;
		ToPose.ResetToRefPose();

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

	// 진행중 from 이 있으면 BlendingFroms 에 push — 폐기하지 않고 multi-blend stack 으로 보존.
	// 그 항목의 Alpha 는 현재 BlendAlpha 그대로 (의미: "FromState → 그 다음 단계 state 로의 진행도"
	// 가 multi 화 후엔 "FromState → 새 BlendingFroms.back() (= 직전 CurrentState) 로의 진행도"
	// 가 되는데, 직전 CurrentState 가 곧 새 from 으로 들어가므로 alpha 값은 그대로 valid).
	// Step 3 에서 Tick/Evaluate 가 이 stack 을 N-pose blend 로 소비. 이 커밋은 push 만 함.
	if (FromState)
	{
		// 상한 초과 시 oldest 강제 정리 — OnExit 후 erase. 일반 게임플레이는 거의 도달 안 함.
		if (BlendingFroms.size() >= static_cast<size_t>(MaxBlendingFroms))
		{
			if (UAnimState* Oldest = BlendingFroms[0].State)
			{
				Oldest->OnExit(Owner);
			}
			BlendingFroms.erase(BlendingFroms.begin());
		}
		BlendingFroms.push_back({ FromState, BlendAlpha, BlendDuration });
	}

	// 이전 블렌드가 끝나기 전 새 전이 — 진행중 From 은 위에서 stack 으로 옮겼고,
	// 현재 상태를 새 From 으로 박는다.
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
