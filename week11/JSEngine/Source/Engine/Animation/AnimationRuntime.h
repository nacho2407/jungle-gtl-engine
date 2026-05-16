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
	 * @brief   skeletal mesh가 가진 bind pose 정보를 TArray<FTransform>의 local pose 형태로 만듦.
	 * 
	 *          현재 skeletal mesh 쪽은 matrix 기반 pose를 사용하는데, animation 쪽에서는 blending 등에서 
	 *          transform 기준 보간을 사용하기 위해 pose를 transform으로 쪼개서 관리하고 있음.
	 * 
	 * @example animation이 없거나 sampling에 실패하고 fallback으로 bind pose를 뱉어줄 때, skeletal mesh의
	 *          bind pose를 animation runtime pose 형식으로 만들어 사용하기 위해 사용
	 */
    static bool BuildBindLocalPoseFromMesh(const USkeletalMesh* Mesh, TArray<FTransform>& OutLocalPose);

	/**
	 * @brief BuildBindLocalPoseFromMesh와는 반대로, animation runtime의 FTransform local pose를
	 *        기존 component가 사용하는 FMatrix local pose로 변환하는 bridge.
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
