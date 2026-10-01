#pragma once

#include "CameraManage/CameraModifiers/CameraModifier.h"

class UCamModifyer_CameraShake : public UCameraModifier
{
public:
    DECLARE_CLASS(UCamModifyer_CameraShake, UCameraModifier)

    void Start(float InDuration, float InLocationAmplitude, float InRotationAmplitude, float InFrequency);
    void Start(const FCameraShakeParams& Params);
    bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

private:
    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    float LocationAmplitude = 0.0f;
    float RotationAmplitude = 0.0f;
    float Frequency = 25.0f;
    float PhaseOffset = 0.0f;
    FCameraFloatCurve LocationCurve;
    FCameraFloatCurve RotationCurve;
};
