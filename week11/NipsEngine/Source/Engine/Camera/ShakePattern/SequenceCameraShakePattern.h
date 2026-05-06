#pragma once
#include "../CameraShakeBase.h"
#include "Animation/TimelinePlayer.h"

class FTimelinePlayer;
class UCameraAnimationSequence;
class UCurveFloatAsset;

enum class ECameraShakeCurveChannel
{
    LocationX = 0,
    LocationY,
    LocationZ,
    Pitch,
    Yaw,
    Roll,
    FOV,
    Count
};

class USequenceCameraShakePattern : public UCameraShakePattern
{
public:
    DECLARE_CLASS(USequenceCameraShakePattern, UCameraShakePattern)

	UCameraAnimationSequence* Sequence = nullptr;
	UCurveFloatAsset* Curve = nullptr;

	float PlayRate = 1.0f;
	float Scale = 1.0f;
    float RandomSegmentDuration = 0.0f;
    bool bRandomSegment = false;
    bool bLoop = false;
    FString CurveAssetPath;

    FVector LocationAmplitude = FVector::ZeroVector;
    FVector RotationAmplitudeDeg = FVector::ZeroVector;
    float FOVAmplitude = 0.0f;

    void GetCameraShakeInfo(FCameraShakeInfo& OutCameraInfo) const override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:	
    virtual void OnStartShakePattern(const FCameraShakeStartParams& Params) override;
    virtual void OnStopShakePattern(bool bImmediately) override;
    virtual void OnUpdateShakePattern(
        const FCameraShakeUpdateParams& Params,
        FCameraShakeUpdateResult& OutResult) override;

private:
    FTimelinePlayer CameraShakeTimeline;
    float CurrentCurveValues[(int)ECameraShakeCurveChannel::Count] = {};
};
