#include "AnimState.h"
#include "AnimInstance.h"
#include "AnimSequenceBase.h"
#include "AnimExtractContext.h"
#include "PoseContext.h"

#include <cmath>

DEFINE_CLASS(UAnimState, UObject)

void UAnimState::Tick(UAnimInstance* Instance, float DeltaSeconds)
{
	if (!Sequence) return;
	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f) return;

	const float PreviousTime = LocalTime;
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

	// FSM 모드에서도 notify dispatch — SingleNode 경로와 일관성. Instance 가 시퀀스 컨텍스트로 dispatch.
	// (UAnimSingleNodeInstance 는 자기 NativeUpdateAnimation 에서 직접 호출. 여기는 FSM 측 채널.)
	if (Instance) Instance->TriggerAnimNotifies(PreviousTime, LocalTime, Sequence);
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
