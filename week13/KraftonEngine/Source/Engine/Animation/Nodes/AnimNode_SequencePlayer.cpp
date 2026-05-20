#include "AnimNode_SequencePlayer.h"

#include "Animation/AnimInstance.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/AnimExtractContext.h"
#include "Animation/PoseContext.h"
#include "Object/Object.h"   // Cast<>

#include <cmath>

void FAnimNode_SequencePlayer::OnBecomeRelevant(const FAnimationInitializeContext& /*Context*/)
{
	// State 진입 / 노드 처음 활성될 때 LocalTime reset. 기존 UAnimState::OnEnter 동작 그대로.
	LocalTime = 0.0f;
}

void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
	if (!Sequence) return;
	const float Length = Sequence->GetPlayLength();
	if (Length <= 0.0f) return;

	const float PreviousTime = LocalTime;
	LocalTime += Context.DeltaSeconds * PlayRate;
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

	// Notify dispatch — 안 보이는 가지의 notify 발사 방지. UAnimInstance 가 큐 적재만 받고
	// UpdateAnimation 끝에서 일괄 dispatch.
	if (Context.AnimInstance && Context.FinalBlendWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		Context.AnimInstance->AddAnimNotifies(PreviousTime, LocalTime, Sequence);
	}

	// Root motion delta 계산 — 부모 노드가 자기 합성 정책으로 모아 AnimInstance 에 누적.
	LastRootMotionDelta = FTransform();
	if (UAnimSequence* Seq = Cast<UAnimSequence>(Sequence))
	{
		if (Seq->GetEnableRootMotion())
		{
			LastRootMotionDelta = Seq->ExtractRootMotion(PreviousTime, LocalTime, bLooping);
		}
	}
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
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
