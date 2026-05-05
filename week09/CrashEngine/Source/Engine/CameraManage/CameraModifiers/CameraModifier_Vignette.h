#pragma once

#include "CameraManage/CameraModifiers/CameraModifier.h"

class UCameraModifier_Vignette : public UCameraModifier
{
public:
    DECLARE_CLASS(UCameraModifier_Vignette, UCameraModifier)

    void Start(const FCameraVignetteParams& Params);
    bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

private:
    float Lerp(float From, float To, float Alpha) const;

    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    float FromIntensity = 0.0f;
    float ToIntensity = 0.0f;
    float Radius = 0.75f;
    float Softness = 0.35f;
};
