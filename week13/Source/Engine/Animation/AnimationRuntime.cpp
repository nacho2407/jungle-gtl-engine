#include "AnimationRuntime.h"
#include "Animation/PoseContext.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float MatrixDecomposeTolerance = 1.0e-6f;
}

FTransform FAnimationRuntime::DecomposeMatrix(const FMatrix& Mat)
{
	FTransform T;
	T.Location = Mat.GetLocation();
	T.Scale    = Mat.GetScale();

	FMatrix Rot = Mat;
	Rot.M[3][0] = 0.0f;
	Rot.M[3][1] = 0.0f;
	Rot.M[3][2] = 0.0f;
	Rot.M[3][3] = 1.0f;

	if (std::fabs(T.Scale.X) > MatrixDecomposeTolerance)
	{
		Rot.M[0][0] /= T.Scale.X;
		Rot.M[0][1] /= T.Scale.X;
		Rot.M[0][2] /= T.Scale.X;
	}
	if (std::fabs(T.Scale.Y) > MatrixDecomposeTolerance)
	{
		Rot.M[1][0] /= T.Scale.Y;
		Rot.M[1][1] /= T.Scale.Y;
		Rot.M[1][2] /= T.Scale.Y;
	}
	if (std::fabs(T.Scale.Z) > MatrixDecomposeTolerance)
	{
		Rot.M[2][0] /= T.Scale.Z;
		Rot.M[2][1] /= T.Scale.Z;
		Rot.M[2][2] /= T.Scale.Z;
	}
	
	const float Det =
	Rot.M[0][0] * (Rot.M[1][1] * Rot.M[2][2] - Rot.M[1][2] * Rot.M[2][1]) -
	Rot.M[0][1] * (Rot.M[1][0] * Rot.M[2][2] - Rot.M[1][2] * Rot.M[2][0]) +
	Rot.M[0][2] * (Rot.M[1][0] * Rot.M[2][1] - Rot.M[1][1] * Rot.M[2][0]);

	if (Det < 0.0f)
	{
		T.Scale.X   = -T.Scale.X;
		Rot.M[0][0] = -Rot.M[0][0];
		Rot.M[0][1] = -Rot.M[0][1];
		Rot.M[0][2] = -Rot.M[0][2];
	}

	T.Rotation = Rot.ToQuat().GetNormalized();
	return T;
}

void FPoseContext::ResetToRefPose()
{
	if (!SkeletalMesh) return;
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset) return;

	const TArray<FBone>& Bones = Asset->Bones;
	Pose.resize(Bones.size());
	for (size_t i = 0; i < Bones.size(); ++i)
	{
		Pose[i] = FAnimationRuntime::DecomposeMatrix(Bones[i].GetReferenceLocalPose());
	}

	MorphWeights.clear();
	MorphWeights.assign(Asset->MorphTargets.size(), 0.0f);
}

void FAnimationRuntime::BlendTwoPosesTogether(
	const FPoseContext& A,
	const FPoseContext& B,
	float Alpha,
	FPoseContext& Out)
{
	const size_t N = A.Pose.size();
	Out.Pose.resize(N);

	for (size_t i = 0; i < N; ++i)
	{
		const FTransform& Ta = A.Pose[i];
		const FTransform& Tb = B.Pose[i];

		FTransform Result;
		Result.Location = Ta.Location + (Tb.Location - Ta.Location) * Alpha;
		// Slerp 인자 정규화 — quat drift 누적 방지. in-place 반복 누적 (LayeredBlend / SM 의 Acc).
		Result.Rotation = FQuat::Slerp(Ta.Rotation.GetNormalized(), Tb.Rotation.GetNormalized(), Alpha).GetNormalized();
		Result.Scale    = Ta.Scale    + (Tb.Scale    - Ta.Scale)    * Alpha;
		Out.Pose[i] = Result;
	}

	const size_t MorphCount = std::max(A.MorphWeights.size(), B.MorphWeights.size());
	Out.MorphWeights.resize(MorphCount, 0.0f);
	for (size_t MorphIndex = 0; MorphIndex < MorphCount; ++MorphIndex)
	{
		const float AV               = MorphIndex < A.MorphWeights.size() ? A.MorphWeights[MorphIndex] : 0.0f;
		const float BV               = MorphIndex < B.MorphWeights.size() ? B.MorphWeights[MorphIndex] : 0.0f;
		Out.MorphWeights[MorphIndex] = AV + (BV - AV) * Alpha;
	}
}
