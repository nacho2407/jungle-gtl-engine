#include "AnimState.h"
#include "AnimInstance.h"
#include "PoseContext.h"
#include "Nodes/AnimNodeContexts.h"

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

void UAnimState::Tick(UAnimInstance* Instance, float DeltaSeconds)
{
	FAnimationUpdateContext Ctx;
	Ctx.AnimInstance     = Instance;
	Ctx.DeltaSeconds     = DeltaSeconds;
	Ctx.FinalBlendWeight = 1.0f;   // FSM 이 자기 blend weight 로 lerp 합성하므로 sub-graph 는 full.

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
