#pragma once
#include "../CameraShakeBase.h"
#include "Animation/TimelinePlayer.h"

#include "SequenceCameraShakePattern.generated.h"

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

UCLASS()
class USequenceCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY_USequenceCameraShakePattern()
public:
    DECLARE_CLASS(USequenceCameraShakePattern, UCameraShakePattern)

	UCameraAnimationSequence* Sequence = nullptr;
	UCurveFloatAsset* Curve = nullptr;

	UPROPERTY(EditAnywhere)
	float PlayRate = 1.0f;
	UPROPERTY(EditAnywhere)
	float Scale = 1.0f;
    UPROPERTY(EditAnywhere)
    float RandomSegmentDuration = 0.0f;
    UPROPERTY(EditAnywhere)
    bool bRandomSegment = false;
    UPROPERTY(EditAnywhere)
    bool bLoop = false;
    FString CurveAssetPath;

    UPROPERTY(EditAnywhere)
    FVector LocationAmplitude = FVector::ZeroVector;
    UPROPERTY(EditAnywhere)
    FVector RotationAmplitudeDeg = FVector::ZeroVector;
    UPROPERTY(EditAnywhere)
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
