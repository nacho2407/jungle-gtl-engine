#pragma once

#include "Object/Object.h"
#include "BoneAnimationTrack.h"
#include "AnimNotifyEvent.h"

// AnimSequence 의 직렬화 가능한 "원본" 데이터 모델.
// 압축/디시메이션은 추후 옵션. 현재는 raw 키프레임 + notify 만 보관.
class UAnimDataModel : public UObject
{
public:
    DECLARE_CLASS(UAnimDataModel, UObject)

    UAnimDataModel()           = default;
    ~UAnimDataModel() override = default;

    void Serialize(FArchive& Ar) override;

    float PlayLength = 0.0f;  // sec
    float FrameRate  = 30.0f; // fps
    int32 NumFrames  = 0;

    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    TArray<FAnimNotifyEvent>    Notifies;

    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const
    {
        return BoneAnimationTracks;
    }

    TArray<FBoneAnimationTrack>& GetMutableBoneAnimationTracks()
    {
        return BoneAnimationTracks;
    }

    int32 GetNumberOfFrames() const
    {
        return NumFrames;
    }

    void SetTiming(float InPlayLength, float InFrameRate, int32 InNumFrames)
    {
        PlayLength = InPlayLength;
        FrameRate  = InFrameRate;
        NumFrames  = InNumFrames;
    }
};
