#pragma once
#include "Core/CoreMinimal.h"
#include "Object/FName.h"

struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;
    float Duration = 0.0f;
    FName NotifyName;
};

struct FPoseContext
{
    TArray<FMatrix> LocalPose;
};