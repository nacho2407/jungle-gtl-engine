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
	(void)Instance;
	LocalTime = 0.0f;

	// 노드 측에도 reset 알림 — phase 1.4 이후 RootNode 경로에서 OnBecomeRelevant 가 일관 활용.
	FAnimationInitializeContext InitCtx;
	InitCtx.AnimInstance = Instance;
	Player.OnBecomeRelevant(InitCtx);
}

void UAnimState::Tick(UAnimInstance* Instance, float DeltaSeconds)
{
	SyncToPlayer(*this, Player);

	FAnimationUpdateContext Ctx;
	Ctx.AnimInstance     = Instance;
	Ctx.DeltaSeconds     = DeltaSeconds;
	Ctx.FinalBlendWeight = 1.0f;   // FSM 이 자기 blend weight 로 lerp 합성하므로 player 는 full.
	Player.Update(Ctx);

	// 내부 → 외부 mirror.
	LocalTime           = Player.LocalTime;
	LastRootMotionDelta = Player.LastRootMotionDelta;
}

void UAnimState::Evaluate(UAnimInstance* /*Instance*/, FPoseContext& Output)
{
	SyncToPlayer(*this, Player);
	Player.Evaluate(Output);
}
