#include "AnimNode_Root.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationMode.h"
#include "Animation/PoseContext.h"

void FAnimNode_Root::Initialize(const FAnimationInitializeContext& Context)
{
	if (ChildPose) ChildPose->Initialize(Context);
}

void FAnimNode_Root::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	if (ChildPose) ChildPose->OnBecomeRelevant(Context);
}

void FAnimNode_Root::OnDormant()
{
	if (ChildPose) ChildPose->OnDormant();
}

void FAnimNode_Root::Update(const FAnimationUpdateContext& Context)
{
	if (!ChildPose) return;

	ChildPose->Update(Context);

	// 트리 평가 후 RootMotion 누적 — AnimInstance 에 있던 분기를 그대로 이동 (정책 단일 진입점).
	// RootMotionFromMontagesOnly 분기는 외부 정책: base graph 측 RM 무시 (Slot 만 적용).
	// AccumulateRootMotion 내부의 IgnoreRootMotion 가드는 그쪽이 책임.
	//
	// 주의: 이전 AnimInstance::UpdateAnimation 에 있던 분기 (`mode != MontagesOnly`) 를 그대로
	// 복원. StateMachine 측에서 이미 MontagesOnly 면 자기 LastRM 을 0 처리하므로 이 외부 분기는
	// 사실상 redundant 로 보이지만, 기존 동작 보존을 위해 그대로 유지.
	if (UAnimInstance* Owner = Context.AnimInstance)
	{
		if (Owner->GetRootMotionMode() != ERootMotionMode::RootMotionFromMontagesOnly)
		{
			Owner->AccumulateRootMotion(ChildPose->GetLastRootMotionDelta());
		}
	}
}

void FAnimNode_Root::Evaluate(FPoseContext& Output)
{
	if (ChildPose)
	{
		ChildPose->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

const FTransform& FAnimNode_Root::GetLastRootMotionDelta() const
{
	// pass-through. 일반적으로 Root 위에 부모는 없지만 (이게 root), 안전 차원.
	static const FTransform Identity;
	return ChildPose ? ChildPose->GetLastRootMotionDelta() : Identity;
}
