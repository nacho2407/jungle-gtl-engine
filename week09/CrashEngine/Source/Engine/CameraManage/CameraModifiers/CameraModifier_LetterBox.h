#pragma once

#include "CameraManage/CameraModifiers/CameraModifier.h"

class UCameraModifier_LetterBox : public UCameraModifier
{
public:
    DECLARE_CLASS(UCameraModifier_LetterBox, UCameraModifier)

    void Start(const FCameraLetterBoxParams& Params);
    bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

private:
    float Lerp(float From, float To, float Alpha) const;
    void NormalizeRatios(float& InOutAppearRatio, float& InOutDisappearRatio) const;
    float GetCurrentAmount() const;

    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    float FromAmount = 0.0f;
    float ToAmount = 0.0f;
    float AppearTime = 0.0f;
    float HoldTime = 0.0f;
    float DisappearTime = 0.0f;
};
