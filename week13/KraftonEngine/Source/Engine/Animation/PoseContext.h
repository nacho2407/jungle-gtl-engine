#pragma once

#include "Core/CoreTypes.h"
#include "Math/Transform.h"

class USkeletalMesh;

// 한 프레임의 본-로컬 transform 묶음. 모든 포즈 입출력의 공통 컨테이너.
//
// - SkeletalMesh 는 본 계층/개수 참조용. 직접 수정하지 않는다.
// - Pose.size() == SkeletalMesh::Bones.size() 를 항상 유지.
// - 좌표계는 부모 본 기준 로컬. ComponentSpace 가 필요하면 호출 측에서 누적.
struct FPoseContext
{
	USkeletalMesh*     SkeletalMesh = nullptr;
	TArray<FTransform> Pose;
	TArray<float>      MorphWeights;

	int32 GetNumBones() const { return static_cast<int32>(Pose.size()); }
	bool  IsValid()     const { return SkeletalMesh != nullptr && !Pose.empty(); }

	// SkeletalMesh 의 bind pose(로컬 행렬) 로 Pose 를 초기화한다.
	// 구현은 AnimationRuntime.cpp — SkeletalMesh 헤더 의존을 피하기 위함.
	void ResetToRefPose();
};
