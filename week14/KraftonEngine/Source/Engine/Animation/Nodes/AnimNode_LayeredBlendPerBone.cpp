#include "AnimNode_LayeredBlendPerBone.h"

#include "Animation/PoseContext.h"
#include "Math/Quat.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"

#include <algorithm>
#include <queue>

void FAnimNode_LayeredBlendPerBone::Initialize(const FAnimationInitializeContext& Context)
{
	if (BasePose)  BasePose->Initialize(Context);
	if (BlendPose) BlendPose->Initialize(Context);
}

void FAnimNode_LayeredBlendPerBone::OnBecomeRelevant(const FAnimationInitializeContext& Context)
{
	if (BasePose)  BasePose->OnBecomeRelevant(Context);
	if (BlendPose) BlendPose->OnBecomeRelevant(Context);
}

void FAnimNode_LayeredBlendPerBone::Update(const FAnimationUpdateContext& Context)
{
	// Base 는 항상 시간 진행 — 보통 locomotion 같은 base layer.
	if (BasePose) BasePose->Update(Context);

	// Blend Update 도 항상 호출 (Slot 내부의 montage time 진행 등). visibility 가드는
	// GetEffectiveBlendWeight 의 weight 가 임계 이상일 때 자식 weight 전파 — Slot 비활성이면
	// effective 0 이라 사실상 inactive 가지.
	if (BlendPose)
	{
		const float Effective = BlendPose->GetEffectiveBlendWeight();
		BlendPose->Update(Context.FractionalWeight(BlendWeight * Effective));
	}

	BaseLastRM = BasePose ? BasePose->GetLastRootMotionDelta() : FTransform();
}

void FAnimNode_LayeredBlendPerBone::Evaluate(FPoseContext& Output)
{
	// 1) BasePose 평가 → Output.
	if (BasePose)
	{
		BasePose->Evaluate(Output);
	}
	else
	{
		Output.ResetToRefPose();
	}

	// 2) Effective weight 계산 — 외부 BlendWeight × BlendPose 의 GetEffectiveBlendWeight.
	//    Slot 의 경우 montage 비활성 → effective 0 → base 100%. 자동 fade.
	if (!BlendPose) return;
	const float W = std::clamp(BlendWeight * BlendPose->GetEffectiveBlendWeight(), 0.0f, 1.0f);
	if (W <= ZERO_ANIMWEIGHT_THRESH) return;

	// 3) BlendPose 평가 → 별 buffer.
	FPoseContext BlendCtx;
	BlendCtx.SkeletalMesh = Output.SkeletalMesh;
	BlendCtx.ResetToRefPose();
	BlendPose->Evaluate(BlendCtx);

	// 4) 본 별 mask 적용. mask[i] == true 인 본만 BlendPose 와 lerp.
	const size_t BoneCount = Output.Pose.size();
	for (size_t i = 0; i < BoneCount; ++i)
	{
		if (i >= PerBoneMask.size() || !PerBoneMask[i]) continue;

		FTransform&       Out   = Output.Pose[i];
		const FTransform& Blend = BlendCtx.Pose[i];

		// Translation / Scale linear, Rotation slerp. BlendTwoPosesTogether 와 동일 식.
		Out.Location = Out.Location + (Blend.Location - Out.Location) * W;
		Out.Rotation = FQuat::Slerp(Out.Rotation.GetNormalized(), Blend.Rotation.GetNormalized(), W).GetNormalized();
		Out.Scale    = Out.Scale    + (Blend.Scale    - Out.Scale)    * W;
	}

	// Morph targets are not bone-scoped in this engine, so the layered node blends the
	// whole morph weight array by the layer weight.
	const size_t MorphCount = std::max(Output.MorphWeights.size(), BlendCtx.MorphWeights.size());
	Output.MorphWeights.resize(MorphCount, 0.0f);
	for (size_t MorphIndex = 0; MorphIndex < MorphCount; ++MorphIndex)
	{
		const float BaseValue = MorphIndex < Output.MorphWeights.size() ? Output.MorphWeights[MorphIndex] : 0.0f;
		const float BlendValue = MorphIndex < BlendCtx.MorphWeights.size() ? BlendCtx.MorphWeights[MorphIndex] : 0.0f;
		Output.MorphWeights[MorphIndex] = BaseValue + (BlendValue - BaseValue) * W;
	}
}

TArray<bool> BuildBoneMaskFromRoot(USkeletalMesh* Mesh, const FString& RootBoneName)
{
	TArray<bool> Mask;
	if (!Mesh) return Mask;
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty()) return Mask;

	const auto& Bones = Asset->Bones;
	Mask.resize(Bones.size(), false);

	// RootBoneName 의 인덱스 찾기 (대소문자 정확 매칭).
	int32 RootIdx = -1;
	for (size_t i = 0; i < Bones.size(); ++i)
	{
		if (Bones[i].Name == RootBoneName) { RootIdx = static_cast<int32>(i); break; }
	}
	if (RootIdx < 0) return Mask;   // not found — 전부 false (BlendPose 무효 = base 100%)

	Mask[RootIdx] = true;

	// BFS — 자손 본 추가. Bones[i].ParentIndex 가 i 의 부모 인덱스.
	std::queue<int32> Q;
	Q.push(RootIdx);
	while (!Q.empty())
	{
		const int32 Parent = Q.front();
		Q.pop();
		for (size_t i = 0; i < Bones.size(); ++i)
		{
			if (Bones[i].ParentIndex == Parent && !Mask[i])
			{
				Mask[i] = true;
				Q.push(static_cast<int32>(i));
			}
		}
	}

	return Mask;
}
