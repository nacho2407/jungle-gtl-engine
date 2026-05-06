#pragma once

#include "GameFramework/CameraShakeBase.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

// ============================================================
// UWaveOscillatorCameraShake — sin 기반 데이터 셰이크.
//
// UE 의 UMatineeCameraShake / UWaveOscillatorCameraShake 의 간소화 버전.
// Location / Rotation / FOV 각 채널마다 amplitude·frequency 로 sin 진동을 합산하고,
// Duration 동안 BlendIn/Out envelope 로 페이드 in/out 한다.
// PlaySpace 변환은 매니저 측에서 처리하므로 서브클래스는 raw additive 만 채운다.
//
// 사용:
//   PC->GetPlayerCameraManager()->StartCameraShake<UWaveOscillatorCameraShake>(1.0f);
// 서브클래스를 만들어 amplitude/frequency 기본값을 바꾸면 다양한 프리셋을 손쉽게 정의 가능.
// ============================================================
class UWaveOscillatorCameraShake : public UCameraShakeBase
{
public:
	DECLARE_CLASS(UWaveOscillatorCameraShake, UCameraShakeBase)

	UWaveOscillatorCameraShake() = default;
	~UWaveOscillatorCameraShake() override = default;

	void StartShake(
		APlayerCameraManager* Camera,
		float InScale,
		ECameraShakePlaySpace InPlaySpace,
		FRotator InUserPlaySpaceRot) override;

	void UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult) override;

	void StopShake(bool bImmediately = true) override;

	// ─── 튜닝 파라미터 ─────────────────────────────────────────────
	// Duration / Blend
	float Duration      = 0.5f;
	float BlendInTime   = 0.05f;
	float BlendOutTime  = 0.10f;

	// Location oscillation (world-space units)
	FVector LocAmplitude = FVector(2.0f, 2.0f, 1.0f);
	FVector LocFrequency = FVector(12.0f, 14.0f, 10.0f);

	// Rotation oscillation (degrees, additive Pitch/Yaw/Roll)
	FRotator RotAmplitude = FRotator(0.5f, 0.8f, 0.8f);
	FRotator RotFrequency = FRotator(11.0f, 13.0f, 9.0f);

	// FOV oscillation (radians, additive)
	float FOVAmplitude = 0.02f;
	float FOVFrequency = 8.0f;

private:
	float ElapsedTime = 0.0f;

	// 매 StartShake 호출 시 randomize → 같은 셰이크 두 번 호출해도 다른 패턴.
	FVector LocPhase   = FVector(0.0f, 0.0f, 0.0f);
	FRotator RotPhase  = FRotator(0.0f, 0.0f, 0.0f);
	float FOVPhase     = 0.0f;
};
