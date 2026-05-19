#pragma once

#include "AnimNode_Base.h"
#include "Object/FName.h"

class UAnimInstance;

// AnimGraph 의 Montage 진입점 노드 (UE 의 FAnimNode_Slot 대응).
//
// Evaluate:
//   1) InputPose 평가 → Output (base pose)
//   2) AnimInstance 에서 SlotName 의 active montage 조회
//   3) 활성이고 BlendWeight > 0 이면 montage pose 평가 후 BlendTwoPosesTogether 로 lerp
//   4) 없거나 weight 0 이면 InputPose 그대로 pass-through (overhead 무)
//
// Montage Tick 은 UAnimInstance::UpdateAnimation 에서 모든 slot 일괄 처리 — Slot 노드의
// Update 는 InputPose Update + LastRM mirror 만. Phase 3 에서 Slot 안으로 Tick 옮길 가능성.
//
// Root motion: InputPose 의 LastRM 그대로 반환. Montage 의 RM 은 UAnimMontageInstance::Tick 안에서
// AnimInstance->AccumulateRootMotion 으로 직접 누적 (현재 패턴 유지). Phase 3 정리 후보.
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
	const char* GetDebugName() const override { return "Slot"; }

private:
	// Slot 이 Evaluate 시 AnimInstance->GetMontageInstanceForSlot 호출하기 위해 Initialize 에서 캐싱.
	// Slot 노드의 lifetime 은 AnimInstance::OwnedNodes 안이라 항상 AnimInstance 보다 짧음 — 안전.
	UAnimInstance* OwnerAnimInstance = nullptr;

	// InputPose 의 LastRM 을 매 Update 후 캐싱 — 부모가 GetLastRootMotionDelta 로 가져감.
	FTransform InputLastRM;
};
