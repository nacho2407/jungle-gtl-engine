#pragma once
#include "../CameraShakeBase.h"
class FTimelinePlayer;
class UCameraAnimationSequence;
class UCurveFloatAsset;

class USequenceCameraShakePattern : public UCameraShakePattern
{
public:
    DECLARE_CLASS(USequenceCameraShakePattern, UCameraShakePattern)

	UCameraAnimationSequence* Sequence;
	float PlayRate;
	float Scale;
    float BlendInTime;
    float BlendOutTime;
    float RandomSegmentDuration;
    bool bRandomSegment;
    FString CurveAssetPath;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:	
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override;
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;
};
