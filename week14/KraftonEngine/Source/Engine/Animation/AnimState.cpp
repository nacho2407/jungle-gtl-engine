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

	// 큐에 적재만 — 실제 dispatch 는 베이스 UAnimInstance::UpdateAnimation 끝에서 1회.
	// (SingleNode 든 FSM 든 자식은 AddAnimNotifies 만 호출, dispatch 책임은 베이스에 통합.)
	if (Instance) Instance->AddAnimNotifies(PreviousTime, LocalTime, Sequence);
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
