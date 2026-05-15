#include "AnimationRuntime.h"
#include "PoseContext.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"

namespace
{
	// 본 로컬 행렬에서 T/R/S 추출. SkeletalMeshAsset 의 FBone::LocalMatrix 는
	// row-major 가정 (FMatrix 의 기존 표기와 일치). 정확도가 중요하지 않은 ref pose
	// 초기화용 fallback — 실제 애니메이션 결과는 항상 FTransform 형태로 들어온다.
	FTransform DecomposeMatrix(const FMatrix& Mat)
	{
		// 단순 구현: Translation 만 추출 (row-major 의 마지막 행), 회전/스케일은 Identity.
		// 정확한 분해가 필요해지면 Math/Transform 에 헬퍼 추가 후 교체.
		FTransform T;
		T.Location = FVector(Mat.M[3][0], Mat.M[3][1], Mat.M[3][2]);
		T.Rotation = FQuat::Identity;
		T.Scale    = FVector(1.0f, 1.0f, 1.0f);
		return T;
	}
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
		Pose[i] = DecomposeMatrix(Bones[i].LocalMatrix);
	}
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
		Result.Rotation = FQuat::Slerp(Ta.Rotation, Tb.Rotation, Alpha);
		Result.Scale    = Ta.Scale    + (Tb.Scale    - Ta.Scale)    * Alpha;
		Out.Pose[i] = Result;
	}
}
