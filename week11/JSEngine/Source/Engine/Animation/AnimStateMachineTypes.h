#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"
#include "Math/Vector2.h"
#include "Object/FName.h"

/**
 * @brief 전이 조건 타입
 */
enum class EAnimConditionType : uint8
{
    None,
    Bool,
    Float,
    LuaFunction,
};

/**
 * @brief 단순 조건 비교용 연산자
 */
enum class EAnimCompareOperator : uint8
{
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
};

struct FAnimStateAnimationRef
{
    FString SourceFbxPath;
    FString AnimStackName;
};

struct FAnimTransitionConditionDesc
{
    EAnimConditionType Type = EAnimConditionType::None;
    FName VariableName;
    EAnimCompareOperator Operator = EAnimCompareOperator::Equal;
    float FloatValue = 0.0f;
    bool BoolValue = false;
    FName LuaFunctionName;
};

struct FAnimStateDesc
{
    int32 Id = -1;
    FName Name;
    FAnimStateAnimationRef Animation;
    bool bLooping = true;
    float PlayRate = 1.0f;

	/**
	 * @brief editor 내 state machine visual editor에서 해당 state node를 배치할 위치
	 * 
	 * @note  visual editor에서만 사용되는 optional data
	 */
    FVector2 EditorPosition = FVector2::ZeroVector;
};

struct FAnimTransitionDesc
{
    int32 Id = -1;
    FName FromState;
    FName ToState;
    float BlendTime = 0.2f;
    int32 Priority = 0;
    bool bCanInterrupt = true;
    bool bCanBeInterrupted = true;
    FAnimTransitionConditionDesc Condition;
};

struct FAnimStateMachineDesc
{
    FString AssetType = "AnimationStateMachine";
    int32 Version = 1;
    FName EntryState;
    TArray<FAnimStateDesc> States;
    TArray<FAnimTransitionDesc> Transitions;

	/**
	 * @brief 상태 전이 source state 이름이 `Any` 또는 `*`인지 검사
	 */
    static bool IsAnyStateName(const FName& StateName);

    FAnimStateDesc* FindStateById(int32 StateId);
    const FAnimStateDesc* FindStateById(int32 StateId) const;
    FAnimStateDesc* FindStateByName(const FName& StateName);
    const FAnimStateDesc* FindStateByName(const FName& StateName) const;
    bool HasState(const FName& StateName) const;

    FAnimTransitionDesc* FindTransitionById(int32 TransitionId);
    const FAnimTransitionDesc* FindTransitionById(int32 TransitionId) const;
    void CollectTransitionsFromState(const FName& FromState, TArray<const FAnimTransitionDesc*>& OutTransitions) const;

	/**
     * @brief 상태 전이 source state 이름이 `Any` 또는 `*`인 trasition desc.를 수집
     */
    void CollectAnyTransitions(TArray<const FAnimTransitionDesc*>& OutTransitions) const;
};
