
#pragma once

#include "Animation/AnimTypes.h"
#include "Object/Object.h"

class UAnimDataModel : public UObject
{
public:
    DECLARE_CLASS(UAnimDataModel, UObject)

    float SequenceLength = 0.0f;
    float FrameRate = 30.0f;
    int32 NumberOfFrames = 0;

    TArray<FAnimationTrack> BoneAnimationTracks;

    const FAnimationTrack* FindTrackByBoneIndex(int32 BoneIndex) const;
    const FAnimationTrack* FindTrackByBoneName(const FName& BoneName) const;
};
