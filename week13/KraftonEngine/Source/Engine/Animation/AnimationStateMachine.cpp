#include "AnimationStateMachine.h"

void UAnimationStateMachine::Tick(UAnimInstance* Owner, float DeltaSeconds)
{
	FAnimationUpdateContext Ctx;
	Ctx.AnimInstance     = Owner;
	Ctx.DeltaSeconds     = DeltaSeconds;
	Ctx.FinalBlendWeight = 1.0f;   // wrapper 직접 호출 = top-level path, full weight.
	Impl.Update(Ctx);
}

void UAnimationStateMachine::Evaluate(UAnimInstance* /*Owner*/, FPoseContext& Output)
{
	Impl.Evaluate(Output);
}
