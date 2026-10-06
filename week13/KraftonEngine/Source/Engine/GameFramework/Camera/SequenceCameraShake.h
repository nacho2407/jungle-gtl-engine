#pragma once
#include "GameFramework/Camera/CameraShakeBase.h"
#include "Object/Ptr/ObjectPtr.h"
#include "FloatCurve/FloatCurveAsset.h"

#include "Source/Engine/GameFramework/Camera/SequenceCameraShake.generated.h"
class UFloatCurveAsset;
class UCameraShakeAsset;

UCLASS()
class USequenceCameraShake : public UCameraShakeBase
{
public:
	GENERATED_BODY()
	USequenceCameraShake() = default;
	~USequenceCameraShake() override = default;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

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

	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> LocXCurve = nullptr;
	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> LocYCurve = nullptr;
	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> LocZCurve = nullptr;

	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> PitchCurve = nullptr;
	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> YawCurve = nullptr;
	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> RollCurve = nullptr;

	UPROPERTY(Transient, Category="CameraShake")
	TObjectPtr<UFloatCurveAsset> FOVCurve = nullptr;

private:
	float ElapsedTime = 0.0f;
};
