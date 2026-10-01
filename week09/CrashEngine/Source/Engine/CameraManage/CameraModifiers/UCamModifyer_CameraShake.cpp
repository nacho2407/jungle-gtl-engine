#include "CameraManage/CameraModifiers/UCamModifyer_CameraShake.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

#include <cmath>

IMPLEMENT_CLASS(UCamModifyer_CameraShake, UCameraModifier)

void UCamModifyer_CameraShake::Start(float InDuration, float InLocationAmplitude, float InRotationAmplitude, float InFrequency)
{
    FCameraShakeParams Params;
    Params.Duration = InDuration;
    Params.LocationAmplitude = InLocationAmplitude;
    Params.RotationAmplitude = InRotationAmplitude;
    Params.Frequency = InFrequency;
    Start(Params);
}

void UCamModifyer_CameraShake::Start(const FCameraShakeParams& Params)
{
    Duration = Params.Duration;
    ElapsedTime = 0.0f;
    LocationAmplitude = Params.LocationAmplitude;
    RotationAmplitude = Params.RotationAmplitude;
    Frequency = Params.Frequency;
    LocationCurve = Params.LocationCurve;
    RotationCurve = Params.RotationCurve;
    if (LocationCurve.Keys.empty())
    {
        LocationCurve.Keys = {{0.0f, 1.0f}, {1.0f, 0.0f}};
    }
    if (RotationCurve.Keys.empty())
    {
        RotationCurve.Keys = {{0.0f, 1.0f}, {1.0f, 0.0f}};
    }
    PhaseOffset += 1.6180339f;
    bFinished = Duration <= 0.0f;
}

bool UCamModifyer_CameraShake::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (bDisabled || bFinished)
    {
        return bFinished;
    }

    ElapsedTime += DeltaTime;
    const float NormalizedTime = Duration > 0.0f ? Clamp(ElapsedTime / Duration, 0.0f, 1.0f) : 1.0f;
    const float Time = ElapsedTime * Frequency + PhaseOffset;

    const float X = std::sinf(Time * 1.31f);
    const float Y = std::sinf(Time * 1.73f + 0.37f);
    const float Z = std::sinf(Time * 2.11f + 0.73f);

    const float LocationStrength = LocationAmplitude * LocationCurve.Evaluate(NormalizedTime);
    InOutView.Location += FVector(X, Y, Z) * LocationStrength;

    const float RotationStrength = RotationAmplitude * RotationCurve.Evaluate(NormalizedTime);
    InOutView.Rotation.Pitch += std::sinf(Time * 1.19f + 0.11f) * RotationStrength;
    InOutView.Rotation.Yaw += std::sinf(Time * 1.47f + 0.43f) * RotationStrength;
    InOutView.Rotation.Roll += std::sinf(Time * 1.91f + 0.79f) * RotationStrength;

    if (ElapsedTime >= Duration)
    {
        Finish();
    }

    return bFinished;
}
