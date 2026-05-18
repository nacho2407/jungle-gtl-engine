#pragma once

#include "Core/Containers/Array.h"
#include "Engine/Geometry/Transform.h"
#include "Math/Matrix.h"

class USkeletalMesh;

/**
 * @brief animation runtime에서 공통으로 쓰는 전역 유틸 함수들을 모은 static utility class
 */
class FAnimationRuntime
{
public:
	/**
	 * @brief animation runtime의 FTransform local pose를 기존 component가 사용하는 FMatrix local pose로 변환하는 bridge.
	 * 
	 *        row-vector convention을 따름에 유의
	 */
    static bool ConvertLocalPoseToMatrices(const TArray<FTransform>& LocalPose, TArray<FMatrix>& OutLocalMatrices);

	/**
	 * @brief skeletal mesh의 bone 개수와 거기에 적용할 animation pose 배열 개수가 일치하는지 검사.
	 * 
	 *        animation asset의 track 개수와 skeletal mesh bone 개수는 충분히 다를 수 있지만,
	 *        거기에 적용되는 최종 animation pose 배열 개수는 맞아야 함
	 */
    static bool HasMatchingBoneCount(const USkeletalMesh* Mesh, const TArray<FTransform>& LocalPose);
};
