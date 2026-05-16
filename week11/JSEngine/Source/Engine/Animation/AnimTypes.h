#pragma once

#include "Core/CoreMinimal.h"
#include "Object/FName.h"
#include "Math/Quat.h"

struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
    TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;

    TArray<float> PosKeyTimes;
    TArray<float> RotKeyTimes;
    TArray<float> ScaleKeyTimes;
};

struct FAnimationTrack
{
    FName BoneName;
    int32 BoneIndex = -1;
    FRawAnimSequenceTrack InternalTrackData;
};
