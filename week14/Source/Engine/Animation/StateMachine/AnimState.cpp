#include "AnimState.h"
#include "Animation/AnimInstance.h"
#include "Animation/PoseContext.h"
#include "Animation/Nodes/AnimNodeContexts.h"

// 외부 public 필드 → 내부 Player 로 동기화.
static void SyncToPlayer(UAnimState& S, FAnimNode_SequencePlayer& Player)
{
	Player.Sequence  = S.Sequence;
	Player.PlayRate  = S.PlayRate;
	Player.bLooping  = S.bLooping;
	Player.LocalTime = S.GetLocalTime();
}

void UAnimState::OnEnter(UAnimInstance* Instance)
{
	LocalTime = 0.0f;

	FAnimationInitializeContext InitCtx;
	InitCtx.AnimInstance = Instance;

	if (SubGraphOverride)
	{
		// Sub-SM 같은 임의 노드 — 자기 init 후크 호출. 자식 SM 이면 자기 current state OnEnter 까지 재귀.
		SubGraphOverride->OnBecomeRelevant(InitCtx);
	}
	else
	{
		Player.OnBecomeRelevant(InitCtx);
	}
}

void UAnimState::OnExit(UAnimInstance* Instance)
{
	(void)Instance;
	// SubGraph 가 SM 인 경우 BlendingFroms 잔여 정리 — 재진입 시 stale alpha 로 시각 pop 방지.
	// SequencePlayer 의 LocalTime 은 다음 OnEnter 에서 reset 되므로 별도 처리 불필요.
	if (SubGraphOverride)
	{
		SubGraphOverride->OnDormant();
	}
}

void UAnimState::Tick(UAnimInstance* Instance, float DeltaSeconds, float Weight)
{
	FAnimationUpdateContext Ctx;
	Ctx.AnimInstance     = Instance;
	Ctx.DeltaSeconds     = DeltaSeconds;
	Ctx.FinalBlendWeight = Weight;   // 부모 SM 이 전달 — BlendingFroms 의 fade-out 비율 반영.

	if (SubGraphOverride)
	{
		SubGraphOverride->Update(Ctx);
		// LastRM mirror — 부모 SM 이 GetLastRootMotionDelta 로 읽음.
		LastRootMotionDelta = SubGraphOverride->GetLastRootMotionDelta();
	}
	else
	{
		SyncToPlayer(*this, Player);
		Player.Update(Ctx);
		LocalTime           = Player.LocalTime;
		LastRootMotionDelta = Player.LastRootMotionDelta;
	}
}

void UAnimState::Evaluate(UAnimInstance* /*Instance*/, FPoseContext& Output)
{
	if (SubGraphOverride)
	{
		SubGraphOverride->Evaluate(Output);
	}
	else
	{
		SyncToPlayer(*this, Player);
		Player.Evaluate(Output);
	}
}
