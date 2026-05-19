#pragma once

#include "AnimNode_Base.h"
#include "Math/Transform.h"

class UAnimInstance;

// AnimGraph 의 entry 노드 (UE 의 FAnimNode_Root 와 동등).
// ChildPose 의 평가/Update 를 pass-through 하면서, AnimInstance 의 RootMotion 누적 정책
// (RootMotionMode) 분기를 자기 안에서 처리한다 — 외부 (UAnimInstance::UpdateAnimation) 에
// 분기 로직 흩어지지 않게.
//
// AnimInstance::SetRootNode 가 자동 wrap — 호출자가 임의 노드를 root 로 박아도 자동으로
// FAnimNode_Root 가 끼워져 정책 단일 진입점 보장.
//
// 향후 확장 지점: Anim Attributes / Curves 후처리, post-process IK, Notify dispatch hook 등
// "트리 평가 후 1회 수행" 작업의 표준 진입점.
class FAnimNode_Root : public FAnimNode_Base
{
public:
	FAnimNode_Base* ChildPose = nullptr;

	void Initialize(const FAnimationInitializeContext& Context) override;
	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void OnDormant() override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	const FTransform& GetLastRootMotionDelta() const override;
	const char*       GetDebugName()           const override { return "Root"; }
	bool              IsRoot()                 const override { return true; }
};
