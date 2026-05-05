#include "CameraManage/CameraModifiers/CameraModifier_Vignette.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier_Vignette, UCameraModifier)

float UCameraModifier_Vignette::Lerp(float From, float To, float Alpha) const
{
    return From + (To - From) * Alpha;
}

void UCameraModifier_Vignette::Start(const FCameraVignetteParams& Params)
{
    Duration = Params.Duration;
    ElapsedTime = 0.0f;
    FromIntensity = Clamp(Params.FromIntensity, 0.0f, 1.0f);
    ToIntensity = Clamp(Params.ToIntensity, 0.0f, 1.0f);
    Radius = Clamp(Params.Radius, 0.0f, 2.0f);
    Softness = Clamp(Params.Softness, 0.001f, 2.0f);
    bFinished = false;
}

bool UCameraModifier_Vignette::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (bDisabled || bFinished)
    {
        return bFinished;
    }

    ElapsedTime += DeltaTime;
    const float Alpha = Duration > 0.0f ? Clamp(ElapsedTime / Duration, 0.0f, 1.0f) : 1.0f;

    InOutView.ScreenEffects.bEnableVignette = true;
    InOutView.ScreenEffects.VignetteIntensity = Lerp(FromIntensity, ToIntensity, Alpha);
    InOutView.ScreenEffects.VignetteRadius = Radius;
    InOutView.ScreenEffects.VignetteSoftness = Softness;

    if (Alpha >= 1.0f)
    {
        Finish();
    }

    return bFinished;
}
