#pragma once

#include "CameraManage/CameraModifiers/CameraModifier.h"

class UCameraModifier_Fade : public UCameraModifier
{
public:
    DECLARE_CLASS(UCameraModifier_Fade, UCameraModifier)

    void Start(const FCameraFadeParams& Params);
    bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

private:
    float Lerp(float From, float To, float Alpha) const;

    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    FVector4 FadeColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    float FromAmount = 0.0f;
    float ToAmount = 0.0f;
};
