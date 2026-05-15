#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Math/Transform.h"

class UAnimInstance;

struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FRotator> RotKeys;
    TArray<FQuat> ScaleKeys;
};

struct FBoneAnimationTrack
{
    FName Name;
    int32 BoneTreeIndex = -1;
};

struct FAnimNotifyEvent
{
    float TriggerTime = 0.0f;
    float Duration    = 0.0f;
    FName NotifyName;
};

struct FAnimCurveValue
{
    FName Name;
    float Value = 0.0f;
};

struct FAnimCurveBuffer
{
    TArray<FAnimCurveValue> Values;
    
    void Reset()
    {
        Values.clear();
    }
};

struct FAnimExtractContext
{
    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;
    
    bool bLooping = false;
    bool bReverse = false;
    
    FAnimExtractContext() = default;
    
    FAnimExtractContext(float InCurrentTime, bool bInLooping): CurrentTime(InCurrentTime), PreviousTime(InCurrentTime), bLooping(bInLooping)
    {
        
    }
    
    FAnimExtractContext(float InCurrentTime, float InPreviiousTime, bool bInLooping, bool bInReverse = false): CurrentTime(InCurrentTime), PreviousTime (InPreviiousTime), bLooping
    (bInLooping), bReverse(bInReverse)
    {
    }
};

struct FPoseContext
{
    UAnimInstance* AnimInstance = nullptr;
    
    TArray<FTransform> Pose;
    FAnimCurveBuffer Curve;
    
    FPoseContext() = default;
    
    explicit FPoseContext(UAnimInstance* InAnimInstance): AnimInstance(InAnimInstance)
    {}
    
    void Reset()
    {
        Pose.clear();
        Curve.Reset();
    }
};