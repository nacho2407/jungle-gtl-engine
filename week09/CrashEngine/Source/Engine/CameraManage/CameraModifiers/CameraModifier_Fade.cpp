#include "CameraManage/CameraModifiers/CameraModifier_Fade.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier_Fade, UCameraModifier)

float UCameraModifier_Fade::Lerp(float From, float To, float Alpha) const
{
    return From + (To - From) * Alpha;
}

void UCameraModifier_Fade::Start(const FCameraFadeParams& Params)
{
    Duration = Params.Duration;
    ElapsedTime = 0.0f;
    FadeColor = Params.Color;
    FromAmount = Clamp(Params.FromAmount, 0.0f, 1.0f);
    ToAmount = Clamp(Params.ToAmount, 0.0f, 1.0f);
    bFinished = false;
}

bool UCameraModifier_Fade::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (bDisabled || bFinished)
    {
        return bFinished;
    }

    ElapsedTime += DeltaTime;
    const float Alpha = Duration > 0.0f ? Clamp(ElapsedTime / Duration, 0.0f, 1.0f) : 1.0f;

    InOutView.ScreenEffects.bEnableFade = true;
    InOutView.ScreenEffects.FadeColor = FadeColor;
    InOutView.ScreenEffects.FadeAmount = Lerp(FromAmount, ToAmount, Alpha);

    if (Alpha >= 1.0f)
    {
        Finish();
    }

    return bFinished;
}
