#include "AnimState.h"
#include "AnimSequenceBase.h"
#include "AnimExtractContext.h"
#include "PoseContext.h"

#include <cmath>

DEFINE_CLASS(UAnimState, UObject)

void UAnimState::Tick(UAnimInstance* /*Instance*/, float DeltaSeconds)
{
	if (!Sequence) return;
	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f) return;

	LocalTime += DeltaSeconds * PlayRate;
	if (bLooping)
	{
		LocalTime = std::fmod(LocalTime, Length);
		if (LocalTime < 0.0f) LocalTime += Length;
	}
	else
	{
		if (LocalTime < 0.0f)   LocalTime = 0.0f;
		if (LocalTime > Length) LocalTime = Length;
	}
}

void UAnimState::Evaluate(UAnimInstance* /*Instance*/, FPoseContext& Output)
{
	if (!Sequence)
	{
		Output.ResetToRefPose();
		return;
	}
	FAnimExtractContext Ctx;
	Ctx.CurrentTime = LocalTime;
	Ctx.bLooping    = bLooping;
	Sequence->GetBonePose(Output, Ctx);
}
