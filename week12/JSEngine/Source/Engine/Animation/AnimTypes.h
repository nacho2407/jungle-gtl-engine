#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;
    FName NotifyName;
};

// 나중에 추가할 수도 있음.
struct FAnimNotifyStateEvent
{
    float TriggerTime = 0.0f;
    float Duration = 0.0f;
    FName NotifyName;
};

struct FPoseContext
{
    // Mesh bone index -> local transform. Animation evaluation writes into this array.
    TArray<FMatrix> LocalPose;

    // Animation track index -> mesh bone index. -1 means the track could not be mapped.
    TArray<int32> TrackToBoneMap;

    // Mesh local bind pose used as the fallback for bones that have no animation track.
    TArray<FMatrix> BindPose;
};