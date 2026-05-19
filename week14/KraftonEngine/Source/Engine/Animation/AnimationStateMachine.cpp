#include "AnimationStateMachine.h"

#include "AnimInstance.h"

void UAnimationStateMachine::Tick(UAnimInstance* Owner, float DeltaSeconds)
{
	FAnimationUpdateContext Ctx;
	Ctx.AnimInstance     = Owner;
	Ctx.DeltaSeconds     = DeltaSeconds;
	Ctx.FinalBlendWeight = 1.0f;   // wrapper 직접 호출 = top-level path, full weight.
	Impl.Update(Ctx);

	// Legacy 경로의 root motion 누적 — 노드 자체는 직접 누적 안 함 (외부 누적 패턴).
	// AnimInstance 의 RootNode 경로와 동일하게 wrapper 가 mode 체크 후 한 번 누적.
	if (Owner && Owner->GetRootMotionMode() != ERootMotionMode::RootMotionFromMontagesOnly)
	{
		Owner->AccumulateRootMotion(Impl.GetLastRootMotionDelta());
	}
}

void UAnimationStateMachine::Evaluate(UAnimInstance* /*Owner*/, FPoseContext& Output)
{
	Impl.Evaluate(Output);
}
