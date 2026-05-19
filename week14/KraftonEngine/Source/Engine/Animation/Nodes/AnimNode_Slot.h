#pragma once

#include "AnimNode_Base.h"
#include "Object/FName.h"

class UAnimInstance;

// AnimGraph 의 Montage 진입점 노드 (UE 의 FAnimNode_Slot 대응).
//
// Update:
//   1) InputPose.Update → InputLastRM mirror
//   2) AnimInstance 의 SlotName 매칭 MontageInstance 조회
//   3) active 면 MontageInstance.Tick(dt) + Slot.LastRM = lerp(InputLastRM, MI.LastRM, W)
//
// Evaluate:
//   1) InputPose.Evaluate → Output (base pose)
//   2) Active + W > 0 이면 MontagePose 평가 후 BlendTwoPosesTogether 로 lerp (in-place 안전)
//   3) 없거나 weight 0 이면 InputPose 그대로 pass-through — overhead 무
//
// 단일 책임 — Slot 이 자기 slot 의 montage 의 모든 처리 (Tick / Evaluate / RM 합성) 를 맡음.
// Root motion 은 외부 누적 패턴이라 직접 AccumulateRootMotion 호출 안 함 — Slot.LastRM 만 채움,
// 부모 (LayeredBlend 또는 RootNode) 가 단일 진입점에서 누적.
//
// EffectiveBlendWeight = montage active 면 montage.GetBlendWeight, 아니면 0.
// LayeredBlend 의 BlendPose 로 박힐 때 이 값이 자동 weight 가 되어 montage 없을 때 base 100%.
class FAnimNode_Slot : public FAnimNode_Base
{
public:
	FName            SlotName;
	FAnimNode_Base*  InputPose = nullptr;

	void Initialize(const FAnimationInitializeContext& Context) override;
	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	const FTransform& GetLastRootMotionDelta() const override { return InputLastRM; }

	// Montage active 면 그 BlendWeight, 아니면 0. LayeredBlend 가 BlendPose 의 이 값을
	// 자동 weight 로 사용해 montage 없을 때 base 100% (UpperBody 데모의 핵심 메커니즘).
	float GetEffectiveBlendWeight() const override;

	const char* GetDebugName() const override { return "Slot"; }

private:
	// Slot 이 Evaluate 시 AnimInstance->GetMontageInstanceForSlot 호출하기 위해 Initialize 에서 캐싱.
	// Slot 노드의 lifetime 은 AnimInstance::OwnedNodes 안이라 항상 AnimInstance 보다 짧음 — 안전.
	UAnimInstance* OwnerAnimInstance = nullptr;

	// InputPose 의 LastRM 을 매 Update 후 캐싱 — 부모가 GetLastRootMotionDelta 로 가져감.
	FTransform InputLastRM;
};
