#pragma once

#include "AnimNode_Base.h"
#include "Core/Types/CoreTypes.h"

class USkeletalMesh;

// Per-bone layered blend (UE 의 FAnimNode_LayeredBoneBlend 대응).
//   mask[i] == false → Output[i] = BasePose[i]
//   mask[i] == true  → Output[i] = lerp(BasePose[i], BlendPose[i], BlendWeight)
//
// Phase 2.6 의 UpperBody 데모 사용처:
//   Root = LayeredBlend(BasePose  = Slot("DefaultSlot" → Locomotion-SM),
//                       BlendPose = Slot("UpperBody"   → RefPose),
//                       Mask      = Spine_01 트리)
//   하반신은 locomotion FSM, 상반신은 UpperBody slot 의 montage.
class FAnimNode_LayeredBlendPerBone : public FAnimNode_Base
{
public:
	FAnimNode_Base* BasePose    = nullptr;
	FAnimNode_Base* BlendPose   = nullptr;

	// size == SkeletalMesh 본 개수. true 인 본은 BlendPose 사용, false 는 BasePose.
	// BuildBoneMaskFromRoot 헬퍼로 본 트리 BFS 채울 수 있음.
	TArray<bool>    PerBoneMask;

	// BlendPose 의 전체 contribution (0..1). 0 이면 BlendPose Update/Evaluate 자체 skip.
	float           BlendWeight = 1.0f;

	void Initialize(const FAnimationInitializeContext& Context) override;
	void OnBecomeRelevant(const FAnimationInitializeContext& Context) override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	// Root motion 은 BasePose 의 LastRM 만 반영 (보통 base 가 locomotion).
	// BlendPose 측 RM 도 weight 합성하려면 phase 3 정리 후보.
	const FTransform& GetLastRootMotionDelta() const override { return BaseLastRM; }
	const char* GetDebugName() const override { return "LayeredBlendPerBone"; }

private:
	FTransform BaseLastRM;
};

// 본 mask helper — RootBoneName 의 인덱스 + 모든 자손 본 인덱스를 true 로 채움.
// 그 외 false. SkeletalMesh 의 Bones[i].ParentIndex 트리를 BFS.
// 본 못 찾으면 전부 false 인 mask (BlendPose 무효 = base 100%).
TArray<bool> BuildBoneMaskFromRoot(USkeletalMesh* Mesh, const FString& RootBoneName);
