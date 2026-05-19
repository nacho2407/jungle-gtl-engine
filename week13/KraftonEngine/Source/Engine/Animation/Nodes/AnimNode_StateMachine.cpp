#include "AnimNode_StateMachine.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimState.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/PoseContext.h"
#include "Math/Quat.h"

void FAnimNode_StateMachine::RegisterState(UAnimState* State)
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

void FAnimNode_StateMachine::RegisterTransition(const FStateTransition& T)
{
	Transitions.push_back(T);
}

void FAnimNode_StateMachine::SetInitialState(FName StateName)
{
	if (UAnimState* S = FindState(StateName))
	{
		CurrentStateName = StateName;
		CurrentState     = S;
	}
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	if (!CurrentState) return;

	UAnimInstance* Owner = Context.AnimInstance;
	const float    DeltaSeconds = Context.DeltaSeconds;

	// 1) 전이 평가 — From 이 현재 상태이거나 AnyState(None) 인 것 중 첫 매치.
	for (const FStateTransition& T : Transitions)
	{
		const bool bMatchesFrom = (T.From == FName::None) || (T.From == CurrentStateName);
		if (!bMatchesFrom) continue;
		if (T.To == CurrentStateName) continue;
		if (T.Condition && T.Condition(Owner))
		{
			BeginBlend(Owner, T.To, T.BlendTime);
			break;
		}
	}

	// 같은 UAnimState 가 CurrentState 와 BlendingFroms 둘 다 (또는 BlendingFroms 안에서 중복)
	// 있는 경우 — A→B→A 같은 빠른 연쇄. 같은 state 에 dt 만큼 Tick 두 번 호출 시 LocalTime
	// 2× 진행 + LastRootMotionDelta 덮어쓰기 → from RM 손실. 가드: 이미 Tick 호출한 state 추적.
	TArray<UAnimState*> Ticked;
	auto TryTick = [&](UAnimState* S)
	{
		if (!S) return;
		for (UAnimState* T : Ticked) if (T == S) return;
		S->Tick(Owner, DeltaSeconds);
		Ticked.push_back(S);
	};

	// 2) 현재 상태 시간 진행.
	TryTick(CurrentState);

	// 3) BlendingFroms 모두 Tick + alpha 증가 + 1.0 도달분 OnExit + 단일 패스 compaction.
	{
		size_t Write = 0;
		for (size_t Read = 0; Read < BlendingFroms.size(); ++Read)
		{
			FBlendingFrom& BF = BlendingFroms[Read];
			TryTick(BF.State);
			if (BF.Duration > 0.0f) BF.Alpha += DeltaSeconds / BF.Duration;
			else                    BF.Alpha = 1.0f;

			if (BF.Alpha >= 1.0f)
			{
				if (BF.State) BF.State->OnExit(Owner);
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

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	if (!CurrentState)
	{
		Output.ResetToRefPose();
		return;
	}

	if (BlendingFroms.empty())
	{
		CurrentState->Evaluate(nullptr, Output);
		return;
	}

	// N-pose sequential lerp.
	//   Acc = BlendingFroms[0].pose
	//   for i: Next = (i+1<N) ? BlendingFroms[i+1].pose : Current.pose
	//          Acc = lerp(Acc, Next, BlendingFroms[i].Alpha)    (in-place 안전)
	FPoseContext Acc;
	Acc.SkeletalMesh = Output.SkeletalMesh;
	Acc.ResetToRefPose();
	BlendingFroms[0].State->Evaluate(nullptr, Acc);

	FPoseContext Scratch;
	Scratch.SkeletalMesh = Output.SkeletalMesh;
	Scratch.ResetToRefPose();

	const size_t N = BlendingFroms.size();
	for (size_t i = 0; i < N; ++i)
	{
		UAnimState* NextState = (i + 1 < N) ? BlendingFroms[i + 1].State : CurrentState;

		Scratch.ResetToRefPose();
		NextState->Evaluate(nullptr, Scratch);

		FAnimationRuntime::BlendTwoPosesTogether(Acc, Scratch, BlendingFroms[i].Alpha, Acc);
	}

	Output = Acc;
}

void FAnimNode_StateMachine::RequestTransition(FName To, float Duration)
{
	BeginBlend(nullptr, To, Duration);
}

UAnimState* FAnimNode_StateMachine::FindState(FName Name) const
{
	for (UAnimState* S : States)
	{
		if (S && S->StateName == Name) return S;
	}
	return nullptr;
}

void FAnimNode_StateMachine::EnterState(UAnimInstance* Owner, FName NewState)
{
	UAnimState* Target = FindState(NewState);
	if (!Target) return;

	if (CurrentState) CurrentState->OnExit(Owner);
	CurrentState     = Target;
	CurrentStateName = NewState;
	CurrentState->OnEnter(Owner);
}

void FAnimNode_StateMachine::BeginBlend(UAnimInstance* Owner, FName NewState, float Duration)
{
	UAnimState* Target = FindState(NewState);
	if (!Target || Target == CurrentState) return;

	// 상한 초과 시 oldest 강제 정리 — OnExit 후 erase.
	if (BlendingFroms.size() >= static_cast<size_t>(MaxBlendingFroms))
	{
		if (UAnimState* Oldest = BlendingFroms[0].State)
		{
			Oldest->OnExit(Owner);
		}
		BlendingFroms.erase(BlendingFroms.begin());
	}

	// 현재 CurrentState 를 새 from 으로 push (alpha=0).
	BlendingFroms.push_back({ CurrentState, 0.0f, Duration });

	CurrentState     = Target;
	CurrentStateName = NewState;
	CurrentState->OnEnter(Owner);

	if (Duration <= 0.0f)
	{
		// Instant cut — 진행중 모든 from 즉시 OnExit + clear.
		for (FBlendingFrom& BF : BlendingFroms)
		{
			if (BF.State) BF.State->OnExit(Owner);
		}
		BlendingFroms.clear();
	}
}
