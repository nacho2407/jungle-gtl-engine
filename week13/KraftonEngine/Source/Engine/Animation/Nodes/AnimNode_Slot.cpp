#include "AnimNode_Slot.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontageInstance.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/PoseContext.h"

void FAnimNode_Slot::Initialize(const FAnimationInitializeContext& Context)
{
	OwnerAnimInstance = Context.AnimInstance;
	if (InputPose) InputPose->Initialize(Context);
}

void FAnimNode_Slot::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	if (InputPose) InputPose->OnBecomeRelevant(Context);
}

void FAnimNode_Slot::Update(const FAnimationUpdateContext& Context)
{
	if (InputPose)
	{
		InputPose->Update(Context);
		InputLastRM = InputPose->GetLastRootMotionDelta();
	}
	else
	{
		InputLastRM = FTransform();
	}
	// Montage Tick 은 UAnimInstance::UpdateAnimation 의 일괄 처리 — 여기 호출 안 함.
}

void FAnimNode_Slot::Evaluate(FPoseContext& Output)
{
	// 1) InputPose 평가 — base pose 가 Output 에 들어감. 없으면 ref pose fallback.
	if (InputPose)
	{
		InputPose->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 2) 이 slot 의 active montage 조회. 없거나 weight 0 이면 pass-through.
	if (!OwnerAnimInstance) return;
	UAnimMontageInstance* MI = OwnerAnimInstance->GetMontageInstanceForSlot(SlotName);
	if (!MI || !MI->IsActive()) return;

	const float Weight = MI->GetBlendWeight();
	if (Weight <= 0.0f) return;

	// 3) Montage pose 평가 후 BlendWeight 로 lerp. BlendTwoPosesTogether 가 in-place 안전
	//    (Output == A 케이스 OK).
	FPoseContext MontagePose;
	MontagePose.SkeletalMesh = Output.SkeletalMesh;
	MontagePose.ResetToRefPose();
	MI->EvaluateMontagePose(MontagePose);

	if (Weight >= 1.0f)
	{
		// 완전 montage — base 무시.
		Output = MontagePose;
	}
	else
	{
		FAnimationRuntime::BlendTwoPosesTogether(Output, MontagePose, Weight, Output);
	}
}
