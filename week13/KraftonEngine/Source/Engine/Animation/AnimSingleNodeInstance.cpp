#include "AnimSingleNodeInstance.h"
#include "AnimSequenceBase.h"
#include "AnimExtractContext.h"

#include <cmath>

DEFINE_CLASS(UAnimSingleNodeInstance, UAnimInstance)

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequenceBase* InAsset)
{
	CurrentAsset = InAsset;
	CurrentTime = 0.0f;
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!bPlaying || !CurrentAsset) return;

	const float PreviousTime = CurrentTime;
	AdvanceTime(DeltaSeconds);
	// 큐에 적재만 — 실제 dispatch 는 베이스 UAnimInstance::UpdateAnimation 끝에서 1회.
	AddAnimNotifies(PreviousTime, CurrentTime, CurrentAsset);
}

void UAnimSingleNodeInstance::EvaluateAnimation(FPoseContext& Output)
{
	if (!CurrentAsset)
	{
		Output.ResetToRefPose();
		return;
	}

	FAnimExtractContext Ctx;
	Ctx.CurrentTime = CurrentTime;
	Ctx.bLooping    = bLooping;
	CurrentAsset->GetBonePose(Output, Ctx);
}

void UAnimSingleNodeInstance::AdvanceTime(float DeltaSeconds)
{
	const float Length = CurrentAsset->GetPlayLength();
	if (Length <= 0.0f) return;

	CurrentTime += DeltaSeconds * PlayRate;

	if (bLooping)
	{
		// std::fmod 는 음수 입력 시 음수를 반환 → 양수로 정규화.
		CurrentTime = std::fmod(CurrentTime, Length);
		if (CurrentTime < 0.0f) CurrentTime += Length;
	}
	else
	{
		if (CurrentTime < 0.0f)    { CurrentTime = 0.0f;    bPlaying = false; }
		if (CurrentTime > Length)  { CurrentTime = Length;  bPlaying = false; }
	}
}
