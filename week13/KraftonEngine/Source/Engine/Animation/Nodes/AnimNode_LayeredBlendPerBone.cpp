#include "AnimNode_LayeredBlendPerBone.h"

#include "Animation/PoseContext.h"
#include "Math/Quat.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"

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
	// Base 는 항상 시간 진행 — 보통 locomotion 같은 base layer, 매 frame 흐름 유지.
	if (BasePose) BasePose->Update(Context);

	// Blend 는 가시성 (BlendWeight × Context.FinalBlendWeight) 가 임계 이상일 때만 진행.
	// 안 보이는 가지의 notify 발사 / RM 누적 막음. FractionalWeight 로 weight 누적 전파.
	if (BlendPose && BlendWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		BlendPose->Update(Context.FractionalWeight(BlendWeight));
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

	// 2) Weight 0 이거나 BlendPose 없으면 base 그대로.
	const float W = std::clamp(BlendWeight, 0.0f, 1.0f);
	if (!BlendPose || W <= ZERO_ANIMWEIGHT_THRESH) return;

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
