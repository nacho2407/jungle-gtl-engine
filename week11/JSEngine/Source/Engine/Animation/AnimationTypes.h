#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Geometry/Transform.h"
#include "Object/FName.h"

class UAnimSequenceBase;

/**
 * @brief USkeletalMeshComponent가 animation을 어떤 방식으로 구동할지 나타내는 실행 모드
 * 
 * @note state machine은 별도 enum 값으로 두지 않고, AnimInstance 모드에서
 *       UAnimStateMachineInstance 같은 UAnimInstance subclass로 처리
 */
enum class EAnimationMode: uint8
{
    None, // animation runtime을 사용하지 않음
    SingleNode, // component가 단일 animation asset 재생용 instance 사용
    AnimInstance, // component가 UAnimInstance subclass에게 animation update / evaluate를 위임
};

/**
 * @brief animation asset에서 특정 시점의 pose를 뽑아낼 때 필요한 문맥
 */
struct FAnimExtractContext
{
    float CurrentTime = 0.0f;
    bool bLooping = false;

    FAnimExtractContext() = default;
    FAnimExtractContext(float InCurrentTime, bool bInLooping)
        : CurrentTime(InCurrentTime)
        , bLooping(bInLooping)
    {
    }
};

/**
 * @brief animation asset timeline에 저장되는 notify 원본 이벤트
 * 
 * @note Duration이 0이면 instant notify, 0보다 크면 duration notify
 */
struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;
    float Duration = 0.0f;
    FName NotifyName;
};

/**
 * @brief notify가 gameplay 쪽의 전달될 때의 실행 단계
 */
enum class EAnimNotifyPhase: uint8
{
    Instant,
    Begin,
    Tick,
    End,
};

/**
 * @brief runtime에서 실제 발생하여 component / actor / gameplay 쪽으로 전달되는 notify 이벤트
 */
struct FAnimNotifyDispatchEvent
{
    FAnimNotifyEvent Notify;
    EAnimNotifyPhase Phase = EAnimNotifyPhase::Instant;
    FName SourceStateName;
    FName SourceAnimationName;
    float TriggerWeight = 1.0f;
    float CurrentTime = 0.0f;
    bool bFromStateMachine = false;
    bool bFromTransitionSource = false;
    bool bFromTransitionTarget = false;
};

/**
 * @brief Begin은 발생했으나 아직 End가 발생하지 않은 duration notify state 추적 데이터
 * 
 * @note NotifyINdex는 sequence 내부 notify 배열에서의 위치. 같은 이름의 notify를 구분하는데에 사용.
 *       state machine에서는 state별 active notifyh를 관리해야하기 때문에 SourceStateName을 함께 보관
 */
struct FActiveAnimNotifyState
{
    int32 NotifyIndex = -1;
    FAnimNotifyEvent Notify;
    FName SourceStateName;
    float LastTriggerWeight = 1.0f;
};

/**
 * @brief 특정 frame에서 animation notify를 검사하기 위해 필요한 입력 문맥(for notify in state machine)
 * 
 * @note state machine transition 중에는 TriggerWeight와 TriggerWeifhtThreshold를 이용하여 충분히
 *       기여중인(weight가 threshold를 넘는) pose source의 notify만 dispatch
 */
struct FAnimNotifyTriggerContext
{
    const UAnimSequenceBase* Sequence = nullptr;
    float PreviousTime = 0.0f;
    float CurrentTime = 0.0f;
    float DeltaSeconds = 0.0f;
    bool bLooping = false;
    bool bReverse = false;
    bool bLooped = false;
    float TriggerWeight = 1.0f;
    float TriggerWeightThreshold = 0.0f;
    FName SourceStateName;
    FName SourceAnimationName;
    bool bFromStateMachine = false;
    bool bFromTransitionSource = false;
    bool bFromTransitionTarget = false;
};

/**
 * @brief animation root bone에서 추출한 root motion을 runtime에서 처리하는 모드
 * 
 * @note ExtractOnly는 delta만 저장하고 owner actor의 transform에 적용은 하지 않음
 */
enum class ERootMotionMode: uint8
{
    Ignore,
    ExtractOnly,
    ApplyToOwner,
};

/**
 * @brief 한 frame 동안 animation root bone에서 추출한 root motion delta
 */
struct FRootMotionDelta
{
    FTransform DeltaTransform = FTransform::Identity;
    FVector Translation = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    bool bHasRootMotion = false;
};
