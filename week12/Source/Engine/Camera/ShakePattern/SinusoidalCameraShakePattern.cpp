#include "SinusoidalCameraShakePattern.h"

static inline float TwoPi()
{
	return 6.28318530717958647692f;
}

void USinusoidalCameraShakePattern::OnStartShakePattern(const FCameraShakeStartParams& Params)
{
}

void USinusoidalCameraShakePattern::OnUpdateShakePattern(
	const FCameraShakeUpdateParams& Params,
	FCameraShakeUpdateResult& OutResult)
{
	const float t = state.ElapsedTime;

	auto axis = [&](float amp, float freq, float phase) -> float
	{
		const float w = TwoPi() * freq;
		return amp * std::sin(w * t + phase);
	};

	OutResult.LocationOffset.X =
		axis(LocationAmplitude.X, std::max(0.0f, LocationFrequency.X), LocationPhase.X);

	OutResult.LocationOffset.Y =
		axis(LocationAmplitude.Y, std::max(0.0f, LocationFrequency.Y), LocationPhase.Y);

	OutResult.LocationOffset.Z =
		axis(LocationAmplitude.Z, std::max(0.0f, LocationFrequency.Z), LocationPhase.Z);

	FRotator RotationDeg;
	RotationDeg.Pitch =
		axis(RotationAmplitudeDeg.X, std::max(0.0f, RotationFrequency.X), RotationPhase.X);

	RotationDeg.Yaw =
		axis(RotationAmplitudeDeg.Y, std::max(0.0f, RotationFrequency.Y), RotationPhase.Y);

	RotationDeg.Roll =
		axis(RotationAmplitudeDeg.Z, std::max(0.0f, RotationFrequency.Z), RotationPhase.Z);

	OutResult.RotationOffset = RotationDeg;

	OutResult.FOVOffset =
		axis(FOVAmplitude, std::max(0.0f, FOVFrequency), FOVPhase);
}
