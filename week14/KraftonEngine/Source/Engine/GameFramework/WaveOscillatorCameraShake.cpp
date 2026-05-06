#include "GameFramework/WaveOscillatorCameraShake.h"
#include "CameraShake/CameraShakeAsset.h"
#include "Object/ObjectFactory.h"
#include <cmath>
#include <cstdlib>

IMPLEMENT_CLASS(UWaveOscillatorCameraShake, UCameraShakeBase)

namespace
{
	constexpr float kTwoPi = 6.28318530717958f;

	// rand() 0..RAND_MAX → [0, 2π) phase
	float RandPhase()
	{
		return (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * kTwoPi;
	}
}

void UWaveOscillatorCameraShake::StartShake(
	APlayerCameraManager* Camera,
	float InScale,
	ECameraShakePlaySpace InPlaySpace,
	FRotator InUserPlaySpaceRot)
{
	Super::StartShake(Camera, InScale, InPlaySpace, InUserPlaySpaceRot);

	ElapsedTime = 0.0f;

	// 같은 셰이크라도 매번 다른 패턴이 나오도록 채널별 위상 랜덤화.
	LocPhase  = FVector(RandPhase(), RandPhase(), RandPhase());
	RotPhase  = FRotator(RandPhase(), RandPhase(), RandPhase());
	FOVPhase  = RandPhase();
}

void UWaveOscillatorCameraShake::ApplyAsset(const UCameraShakeAsset* ShakeAsset)
{
	if (!ShakeAsset)
	{
		return;
	}

	Duration = ShakeAsset->Duration;
	BlendInTime = ShakeAsset->BlendInTime;
	BlendOutTime = ShakeAsset->BlendOutTime;
	bSingleInstance = ShakeAsset->bSingleInstance;

	LocAmplitude = ShakeAsset->WaveOscillator.LocationAmplitude;
	LocFrequency = ShakeAsset->WaveOscillator.LocationFrequency;

	RotAmplitude = ShakeAsset->WaveOscillator.RotationAmplitude;
	RotFrequency = ShakeAsset->WaveOscillator.RotationFrequency;

	FOVAmplitude = ShakeAsset->WaveOscillator.FOVAmplitude;
	FOVFrequency = ShakeAsset->WaveOscillator.FOVFrequency;
}

void UWaveOscillatorCameraShake::UpdateAndApplyCameraShake(float DeltaTime, FCameraShakeUpdateResult& OutResult)
{
	if (bFinished || Duration <= 0.0f) return;

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= Duration)
	{
		bFinished = true;
		return;
	}

	// Blend envelope — Duration 양 끝에서 amplitude 가 0 으로 부드럽게 ramp.
	float Envelope = 1.0f;
	if (BlendInTime > 0.0f && ElapsedTime < BlendInTime)
	{
		Envelope = ElapsedTime / BlendInTime;
	}
	else if (BlendOutTime > 0.0f && ElapsedTime > Duration - BlendOutTime)
	{
		Envelope = (Duration - ElapsedTime) / BlendOutTime;
	}

	const float t = ElapsedTime;
	const float Gain = Scale * Envelope;

	// Location (world-space — PlaySpace 변환은 매니저 책임. 현재는 raw 가산).
	OutResult.Location.X += std::sin(t * LocFrequency.X + LocPhase.X) * LocAmplitude.X * Gain;
	OutResult.Location.Y += std::sin(t * LocFrequency.Y + LocPhase.Y) * LocAmplitude.Y * Gain;
	OutResult.Location.Z += std::sin(t * LocFrequency.Z + LocPhase.Z) * LocAmplitude.Z * Gain;

	// Rotation (degrees, additive Pitch/Yaw/Roll).
	OutResult.Rotation.Pitch += std::sin(t * RotFrequency.Pitch + RotPhase.Pitch) * RotAmplitude.Pitch * Gain;
	OutResult.Rotation.Yaw   += std::sin(t * RotFrequency.Yaw   + RotPhase.Yaw)   * RotAmplitude.Yaw   * Gain;
	OutResult.Rotation.Roll  += std::sin(t * RotFrequency.Roll  + RotPhase.Roll)  * RotAmplitude.Roll  * Gain;

	// FOV (radians, additive).
	OutResult.FOV += std::sin(t * FOVFrequency + FOVPhase) * FOVAmplitude * Gain;
}

void UWaveOscillatorCameraShake::StopShake(bool bImmediately)
{
	if (bImmediately)
	{
		bFinished = true;
		return;
	}

	// Soft stop — BlendOut 만 남기고 Duration 을 ElapsedTime 까지 단축.
	// 이미 BlendOut 구간이거나 Duration 이 너무 짧으면 즉시 종료.
	if (BlendOutTime > 0.0f && ElapsedTime + BlendOutTime < Duration)
	{
		Duration = ElapsedTime + BlendOutTime;
	}
	else
	{
		bFinished = true;
	}
}
