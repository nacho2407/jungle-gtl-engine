#pragma once

#include "Object/Object.h"
#include "Animation/Sequence/BoneAnimationTrack.h"
#include "Animation/Notify/AnimNotifyEvent.h"

// AnimSequence 의 직렬화 가능한 "원본" 데이터 모델.
// 압축/디시메이션은 추후 옵션. 현재는 raw 키프레임 + notify 만 보관.

#include "Source/Engine/Animation/Sequence/AnimDataModel.generated.h"

UCLASS()
class UAnimDataModel : public UObject
{
public:
	GENERATED_BODY()
    UAnimDataModel()           = default;
    ~UAnimDataModel() override = default;

    void Serialize(FArchive& Ar) override;

    float PlayLength = 0.0f;  // sec
    float FrameRate  = 30.0f; // fps
    int32 NumFrames  = 0;

    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    TArray<FMorphTargetCurve>   MorphTargetCurves;
    TArray<FAnimNotifyEvent>    Notifies;

    const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const
    {
        return BoneAnimationTracks;
    }

    TArray<FBoneAnimationTrack>& GetMutableBoneAnimationTracks()
    {
        return BoneAnimationTracks;
    }

    const TArray<FMorphTargetCurve>& GetMorphTargetCurves() const
    {
        return MorphTargetCurves;
    }

    TArray<FMorphTargetCurve>& GetMutableMorphTargetCurves()
    {
        return MorphTargetCurves;
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
