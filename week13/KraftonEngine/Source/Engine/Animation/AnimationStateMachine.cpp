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

	// 3) BlendingFroms 모두 Tick + alpha 증가 + 1.0 도달분 OnExit + 단일 패스 compaction.
	//    Step 6 에서 같은 state 중복 Tick 방지 가드 추가 예정 (A→B→A 케이스).
	{
		size_t Write = 0;
		for (size_t Read = 0; Read < BlendingFroms.size(); ++Read)
		{
			FBlendingFrom& BF = BlendingFroms[Read];
			if (BF.State) BF.State->Tick(Owner, DeltaSeconds);
			if (BF.Duration > 0.0f) BF.Alpha += DeltaSeconds / BF.Duration;
			else                    BF.Alpha = 1.0f;

			if (BF.Alpha >= 1.0f)
			{
				if (BF.State) BF.State->OnExit(Owner);
				// drop — Write 증가 안 함.
			}
			else
			{
				if (Write != Read) BlendingFroms[Write] = BF;
				++Write;
			}
		}
		BlendingFroms.resize(Write);
	}

	// 4) Root motion delta 누적 — sequential lerp 합성.
	//    Acc = BlendingFroms[0].delta, 그 후 차례로 Acc = lerp(Acc, Next.delta, alpha[i]).
	//    Next = (i+1<N) ? BlendingFroms[i+1].delta : CurrentState.delta.
	//    RootMotionFromMontagesOnly mode 일 때 FSM base 누적 skip (Montage 만 적용되도록).
	if (Owner && Owner->GetRootMotionMode() != ERootMotionMode::RootMotionFromMontagesOnly)
	{
		if (BlendingFroms.empty())
		{
			Owner->AccumulateRootMotion(CurrentState->GetLastRootMotionDelta());
		}
		else
		{
			FTransform Acc = BlendingFroms[0].State->GetLastRootMotionDelta();
			for (size_t i = 0; i < BlendingFroms.size(); ++i)
			{
				const FTransform& Next = (i + 1 < BlendingFroms.size())
					? BlendingFroms[i + 1].State->GetLastRootMotionDelta()
					: CurrentState->GetLastRootMotionDelta();
				const float a = BlendingFroms[i].Alpha;
				Acc.Location = Acc.Location * (1.0f - a) + Next.Location * a;
				Acc.Rotation = FQuat::Slerp(Acc.Rotation.GetNormalized(),
				                            Next.Rotation.GetNormalized(), a).GetNormalized();
			}
			Owner->AccumulateRootMotion(Acc);
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

	if (BlendingFroms.empty())
	{
		CurrentState->Evaluate(Owner, Output);
		return;
	}

	// N-pose sequential lerp 합성.
	//   Acc = BlendingFroms[0].pose
	//   for i in 0..N-1:
	//     Next = (i+1 < N) ? BlendingFroms[i+1].pose : CurrentState.pose
	//     Acc = lerp(Acc, Next, BlendingFroms[i].Alpha)   ← in-place 안전 (BlendTwoPosesTogether 가 본 i 끼리만 참조)
	//
	// size 1 검증: BlendingFroms = [{F0, α}], 루프 i=0 1회: Next=Current, Acc = lerp(F0, Cur, α)
	//   → 기존 단일-from 식 lerp(FromPose, ToPose, BlendAlpha) 와 정확 동치.
	//
	// ★ ResetToRefPose 필수 — Sequence->GetBonePose 는 트랙 있는 본만 덮어씀.
	//   ref pose 로 시작 안 하면 트랙 없는 본이 default FTransform 으로 남아 lerp 시
	//   본들이 부모 기준 (0,0,0) 으로 끌려감 ("바닥에 꼬꾸라짐").

	FPoseContext Acc;
	Acc.SkeletalMesh = Output.SkeletalMesh;
	Acc.ResetToRefPose();
	BlendingFroms[0].State->Evaluate(Owner, Acc);

	FPoseContext Scratch;
	Scratch.SkeletalMesh = Output.SkeletalMesh;
	Scratch.ResetToRefPose();   // 첫 reuse 전에 size 잡힘.

	const size_t N = BlendingFroms.size();
	for (size_t i = 0; i < N; ++i)
	{
		UAnimState* NextState = (i + 1 < N) ? BlendingFroms[i + 1].State : CurrentState;

		Scratch.ResetToRefPose();
		NextState->Evaluate(Owner, Scratch);

		FAnimationRuntime::BlendTwoPosesTogether(Acc, Scratch, BlendingFroms[i].Alpha, Acc);
	}

	Output = Acc;
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

	// 상한 초과 시 oldest 강제 정리 — OnExit 후 erase. 일반 게임플레이는 거의 도달 안 함.
	if (BlendingFroms.size() >= static_cast<size_t>(MaxBlendingFroms))
	{
		if (UAnimState* Oldest = BlendingFroms[0].State)
		{
			Oldest->OnExit(Owner);
		}
		BlendingFroms.erase(BlendingFroms.begin());
	}

	// 현재 CurrentState 를 새 from 으로 BlendingFroms 에 push (alpha=0).
	// 진행중 from 들도 그대로 stack 에 남아 Tick/Evaluate 가 모두 합성.
	BlendingFroms.push_back({ CurrentState, 0.0f, Duration });

	CurrentState     = Target;
	CurrentStateName = NewState;
	CurrentState->OnEnter(Owner);

	if (Duration <= 0.0f)
	{
		// Instant cut — 위에서 push 한 항목 즉시 OnExit + pop. Step 6 에서 진행중 다른 from
		// 들도 같이 cleanup 하는 정책 추가 예정.
		if (BlendingFroms.back().State)
		{
			BlendingFroms.back().State->OnExit(Owner);
		}
		BlendingFroms.pop_back();
	}
}
