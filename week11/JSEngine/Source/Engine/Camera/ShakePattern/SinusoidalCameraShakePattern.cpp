#include "SinusoidalCameraShakePattern.h"

DEFINE_CLASS(USinusoidalCameraShakePattern, UCameraShakePattern)
REGISTER_FACTORY(USinusoidalCameraShakePattern)

static inline float TwoPi()
{
    return 6.28318530717958647692f;
}

void USinusoidalCameraShakePattern::OnStartShakePattern(const FCameraShakeStartParams& Params)
{
}

void USinusoidalCameraShakePattern::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UCameraShakePattern::GetEditableProperties(OutProps);

    OutProps.push_back({ "LocationAmplitude", EPropertyType::Vec3, &LocationAmplitude });
    OutProps.push_back({ "LocationFrequency", EPropertyType::Vec3, &LocationFrequency });
    OutProps.push_back({ "LocationPhase", EPropertyType::Vec3, &LocationPhase });
    OutProps.push_back({ "RotationAmplitudeDeg", EPropertyType::Vec3, &RotationAmplitudeDeg });
    OutProps.push_back({ "RotationFrequency", EPropertyType::Vec3, &RotationFrequency });
    OutProps.push_back({ "RotationPhase", EPropertyType::Vec3, &RotationPhase });
    OutProps.push_back({ "FOVAmplitude", EPropertyType::Float, &FOVAmplitude });
    OutProps.push_back({ "FOVFrequency", EPropertyType::Float, &FOVFrequency });
    OutProps.push_back({ "FOVPhase", EPropertyType::Float, &FOVPhase });
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
