#pragma once

#include "AnimNode_Base.h"
#include "Math/Transform.h"

class UAnimSequenceBase;

// 단일 sequence 재생 노드 — AnimGraph 의 leaf.
//   Update: LocalTime 진행 + AddAnimNotifies (weight > threshold 일 때만) + LastRootMotionDelta 계산.
//   Evaluate: Sequence->GetBonePose 호출해 Output 채움.
//
// Root motion: SequencePlayer 자체는 AnimInstance 에 직접 누적 안 함. 부모 (StateMachine 등) 가
// 자기 자식들의 LastRootMotionDelta 를 자체 합성 정책 (예: multi-blend sequential lerp) 으로
// 모아 AnimInstance->AccumulateRootMotion 호출. SequencePlayer 가 트리의 root 인 경우는
// AnimInstance 가 RootNode 평가 후 직접 가져가는 옵션 — phase 1.4 에서 정리.
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
