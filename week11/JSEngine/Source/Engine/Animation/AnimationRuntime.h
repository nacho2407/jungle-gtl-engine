#pragma once

#include "Animation/AnimationTypes.h"
#include "Core/Containers/Array.h"
#include "Engine/Geometry/Transform.h"
#include "Math/Matrix.h"

#include <functional>

class USkeletalMesh;

/**
 * @brief animation runtime에서 공통으로 쓰는 전역 유틸 함수들을 모은 static utility class
 */
class FAnimationRuntime
{
public:
    using FAnimNotifyDispatchFunction = std::function<void(const FAnimNotifyDispatchEvent&)>;

	/**
	 * @brief animation runtime의 FTransform local pose를 기존 component가 사용하는 FMatrix local pose로 변환하는 bridge.
	 * 
	 *        row-vector convention을 따름에 유의
	 */
    static bool ConvertLocalPoseToMatrices(const TArray<FTransform>& LocalPose, TArray<FMatrix>& OutLocalMatrices);

	/**
	 * @brief 두 개의 local pose를 Alpha 비율로 섞어서 최종 local pose를 만드는 함수
	 */
    static bool BlendLocalPoses(
        const TArray<FTransform>& PoseA,
        const TArray<FTransform>& PoseB,
        float Alpha,
        TArray<FTransform>& OutPose);

	/**
	 * @brief 특정 animation sequence의 time 구간을 검사하여 그 사이 발생하는 notify를 dispatch
	 */
    static void TriggerAnimNotifies(
        const FAnimNotifyTriggerContext& Context,
        TArray<FActiveAnimNotifyState>& InOutActiveNotifyStates,
        const FAnimNotifyDispatchFunction& DispatchFunc);

	/**
	 * @brief 현재 active 상태인 모든 duration notify state 정리
	 */
    static void ClearActiveAnimNotifyStates(
        TArray<FActiveAnimNotifyState>& InOutActiveNotifyStates,
        const FAnimNotifyDispatchFunction& DispatchFunc,
        bool bDispatchEnd,
        const FAnimNotifyTriggerContext& Context);

	/**
	 * @brief skeletal mesh의 bone 개수와 거기에 적용할 animation pose 배열 개수가 일치하는지 검사.
	 * 
	 *        animation asset의 track 개수와 skeletal mesh bone 개수는 충분히 다를 수 있지만,
	 *        거기에 적용되는 최종 animation pose 배열 개수는 맞아야 함
	 */
    static bool HasMatchingBoneCount(const USkeletalMesh* Mesh, const TArray<FTransform>& LocalPose);
};
