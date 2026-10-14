#include "AnimNode_Root.h"

#include "Animation/AnimInstance.h"
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

	// 트리 평가 후 RootMotion 누적 — RootMotionFromMontagesOnly 일 때도 호출.
	// StateMachine 가 그 모드에서 자기 LastRM 을 Identity 로 처리하고, Slot 은
	// lerp(0, montageRM, w) = montageRM*w 만 노출하므로 ChildPose.LastRM 자체가 이미
	// "montage 만" 의 결과. 외부에서 또 분기로 막으면 montage RM 도 누적 안 되는
	// 버그가 됨. IgnoreRootMotion 가드는 AccumulateRootMotion 내부에서 처리.
	if (UAnimInstance* Owner = Context.AnimInstance)
	{
		Owner->AccumulateRootMotion(ChildPose->GetLastRootMotionDelta());
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
