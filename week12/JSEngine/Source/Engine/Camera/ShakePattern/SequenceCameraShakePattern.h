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

UCLASS()
class USequenceCameraShakePattern : public UCameraShakePattern
{
public:
	GENERATED_BODY(USequenceCameraShakePattern, UCameraShakePattern)

	UCameraAnimationSequence* Sequence = nullptr;
	UCurveFloatAsset* Curve = nullptr;

	UPROPERTY()
	float PlayRate = 1.0f;

	UPROPERTY()
	float Scale = 1.0f;

	UPROPERTY()
	float RandomSegmentDuration = 0.0f;

	UPROPERTY()
	bool bRandomSegment = false;

	UPROPERTY()
	bool bLoop = false;

	UPROPERTY()
	FString CurveAssetPath;

	UPROPERTY()
	FVector LocationAmplitude = FVector::ZeroVector;

	UPROPERTY()
	FVector RotationAmplitudeDeg = FVector::ZeroVector;

	UPROPERTY()
	float FOVAmplitude = 0.0f;

	void GetCameraShakeInfo(FCameraShakeInfo& OutCameraInfo) const override;
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
