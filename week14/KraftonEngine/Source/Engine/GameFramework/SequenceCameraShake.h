#pragma once
#include "CameraShakeBase.h"

class UFloatCurveAsset;
class UCameraShakeAsset;

class USequenceCameraShake : public UCameraShakeBase
{
public:
	DECLARE_CLASS(USequenceCameraShake, UCameraShakeBase)

	USequenceCameraShake() = default;
	~USequenceCameraShake() override = default;

	void StartShake(
		APlayerCameraManager* Camera,
		float InScale,
		ECameraShakePlaySpace InPlaySpace,
		FRotator InUserPlaySpaceRot) override;

	void UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult) override;

	void StopShake(bool bImmediately = true) override;

	void ApplyAsset(const UCameraShakeAsset* ShakeAsset);

	float Duration = 0.5f;
	float BlendInTime = 0.05f;
	float BlendOutTime = 0.10f;

	UFloatCurveAsset* LocXCurve = nullptr;
	UFloatCurveAsset* LocYCurve = nullptr;
	UFloatCurveAsset* LocZCurve = nullptr;

	UFloatCurveAsset* PitchCurve = nullptr;
	UFloatCurveAsset* YawCurve = nullptr;
	UFloatCurveAsset* RollCurve = nullptr;

	UFloatCurveAsset* FOVCurve = nullptr;

private:
	float ElapsedTime = 0.0f;
};
