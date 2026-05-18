#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Geometry/Transform.h"
#include "Object/FName.h"

enum class EAnimationMode : uint8
{
    None,
    SingleNode,
    AnimInstance,
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

struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;
    float Duration = 0.0f;
    FName NotifyName;
};

enum class EAnimNotifyPhase : uint8
{
    Instant,
    Begin,
    Tick,
    End,
};

struct FAnimNotifyDispatchEvent
{
    FAnimNotifyEvent Notify;
    EAnimNotifyPhase Phase = EAnimNotifyPhase::Instant;
};

enum class ERootMotionMode : uint8
{
    Ignore,
    ExtractOnly,
    ApplyToOwner,
};

struct FRootMotionDelta
{
    FTransform DeltaTransform = FTransform::Identity;
    FVector Translation = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    bool bHasRootMotion = false;
};
