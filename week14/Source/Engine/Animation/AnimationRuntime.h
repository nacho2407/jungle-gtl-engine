#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"

struct FPoseContext;
struct FMatrix;

// 포즈 연산 정적 라이브러리. UObject 가 아니다.
namespace FAnimationRuntime
{
	// 본별 transform 을 선형 보간.
	//   Location/Scale → Lerp
	//   Rotation       → Slerp (★ 사원수 Lerp 는 잘못된 방향 보간 — 반드시 Slerp)
	//
	// A.Pose.size() == B.Pose.size() == Out.Pose.size() 가정.
	// Alpha 0 → A, 1 → B, [0,1] 외 범위는 호출자 책임.
	void BlendTwoPosesTogether(
		const FPoseContext& A,
		const FPoseContext& B,
		float Alpha,
		FPoseContext& Out);
	
	// 본별 weight 로 두 pose 를 보간한다.
	// BoneWeights[i] == 0 → A pose, 1 → B pose.
	// BoneWeights 가 비어 있거나 특정 bone weight 가 없으면 0 으로 보고 A pose 를 유지한다.
	//
	// Ragdoll blend 에서는 A = animation pose, B = physics pose 로 사용한다.
	// Morph target 은 bone별 물리 weight 와 직접 대응되지 않으므로 A 쪽 값을 유지한다.
	void BlendTwoPosesPerBone(
		const FPoseContext& A,
		const FPoseContext& B,
		const TArray<float>& BoneWeights,
		FPoseContext& Out);

	// 본 로컬 행렬 → FTransform 분해. row-major 가정. row 별 scale 을 제거한 뒤 회전을 추출.
	// FBone::LocalMatrix 같은 bind-pose 행렬을 Animation 자료구조 (FTransform) 로 옮길 때 사용.
	FTransform DecomposeMatrix(const FMatrix& Mat);
}
