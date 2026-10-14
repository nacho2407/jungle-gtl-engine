#pragma once

#include "AnimNode_Base.h"
#include "Math/Transform.h"

class UAnimSequenceBase;

// 단일 sequence 재생 노드 — AnimGraph 의 leaf.
//   Update: LocalTime 진행 + AddAnimNotifies (Context.FinalBlendWeight > THRESH 일 때만)
//           + LastRootMotionDelta 계산.
//   Evaluate: Sequence->GetBonePose 호출해 Output 채움.
//
// Root motion 은 외부 누적 패턴 — SequencePlayer 는 LastRootMotionDelta 만 채움, 직접
// AccumulateRootMotion 호출 X. 부모 (StateMachine / Slot / LayeredBlend) 가 자기 자식의
// LastRM 을 합성 후 자기 LastRM 으로, 결국 RootNode 단일 진입점에서 한 번 누적.
class FAnimNode_SequencePlayer : public FAnimNode_Base
{
public:
	UAnimSequenceBase* Sequence  = nullptr;
	float              PlayRate  = 1.0f;
	bool               bLooping  = true;

	float              LocalTime = 0.0f;
	FTransform         LastRootMotionDelta;

	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
	const FTransform& GetLastRootMotionDelta() const override { return LastRootMotionDelta; }

	const char* GetDebugName() const override { return "SequencePlayer"; }
};
