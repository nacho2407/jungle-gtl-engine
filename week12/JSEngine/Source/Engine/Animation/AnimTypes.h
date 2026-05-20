#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

#include <algorithm>

struct FAnimNotifyStateEvent
{
    float TriggerTime = 0.0f;
    float Duration = 0.0f;
    FName NotifyName;
    FString NotifyClassName;

    float GetEndTime() const
    {
        return TriggerTime + std::max(0.0f, Duration);
    }

    bool IsState() const
    {
        return Duration > 0.0f;
    }

    FString GetDisplayName() const
    {
        return NotifyName.IsValid() ? NotifyName.ToString() : NotifyClassName;
    }
};

// Legacy compatibility: old code that still names this as FAnimNotifyEvent now uses the
// state-capable event payload. Duration == 0 means a one-shot notify.
using FAnimNotifyEvent = FAnimNotifyStateEvent;

struct FPoseContext
{
    // Mesh bone index -> local transform. Animation evaluation writes into this array.
    TArray<FMatrix> LocalPose;

    // Animation track index -> mesh bone index. -1 means the track could not be mapped.
    TArray<int32> TrackToBoneMap;

    // Mesh local bind pose used as the fallback for bones that have no animation track.
    TArray<FMatrix> BindPose;
};
