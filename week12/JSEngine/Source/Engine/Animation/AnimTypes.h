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
    TArray<FMatrix> LocalPose;
};