#pragma once

#include "CameraManage/CameraModifiers/CameraModifier.h"

class UCameraModifier_GammaCorrection : public UCameraModifier
{
public:
    DECLARE_CLASS(UCameraModifier_GammaCorrection, UCameraModifier)

    void Start(const FCameraGammaCorrectionParams& Params);
    bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

private:
    float Lerp(float From, float To, float Alpha) const;

    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    float FromGamma = 1.0f;
    float ToGamma = 1.0f;
};
