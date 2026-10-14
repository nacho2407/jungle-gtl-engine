#pragma once

class UAnimInstance;
class USkeletalMesh;

// FAnimNode 트리 build 직후 1 회 호출되는 init context — 자식의 Initialize 도 부모가 재귀 전달.
struct FAnimationInitializeContext
{
	UAnimInstance* AnimInstance = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
};

// 매 frame Update 시 전달. Pose 는 안 들고 다님 (Evaluate 만 만짐).
// 부모 노드가 자식 호출 시 FractionalWeight() 로 weight 를 누적해 새 ctx 전달 — Slot/LayeredBlend 가
// 자식의 visible 가중치를 정확히 계산해 Notify trigger 임계값 / RootMotion scale 에 반영하도록.
struct FAnimationUpdateContext
{
	UAnimInstance* AnimInstance     = nullptr;
	float          DeltaSeconds     = 0.0f;
	float          FinalBlendWeight = 1.0f;   // 부모 누적 W × 자기 노드 fraction.

	FAnimationUpdateContext FractionalWeight(float Fraction) const
	{
		FAnimationUpdateContext Sub = *this;
		Sub.FinalBlendWeight *= Fraction;
		return Sub;
	}
};

// Notify trigger / RM 누적의 가시성 임계값. UE 의 ZERO_ANIMWEIGHT_THRESH 와 동일 의미.
// FinalBlendWeight 가 이보다 작으면 사실상 안 보이는 가지 — Tick 단계에서 dispatch / 누적 skip.
inline constexpr float ZERO_ANIMWEIGHT_THRESH = 1e-4f;
