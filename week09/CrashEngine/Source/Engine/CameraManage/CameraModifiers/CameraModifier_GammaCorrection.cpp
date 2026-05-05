#include "CameraManage/CameraModifiers/CameraModifier_GammaCorrection.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier_GammaCorrection, UCameraModifier)

float UCameraModifier_GammaCorrection::Lerp(float From, float To, float Alpha) const
{
    return From + (To - From) * Alpha;
}

void UCameraModifier_GammaCorrection::Start(const FCameraGammaCorrectionParams& Params)
{
    Duration = Params.Duration;
    ElapsedTime = 0.0f;
    FromGamma = Clamp(Params.FromGamma, 0.01f, 8.0f);
    ToGamma = Clamp(Params.ToGamma, 0.01f, 8.0f);
    bFinished = false;
}

bool UCameraModifier_GammaCorrection::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (bDisabled || bFinished)
    {
        return bFinished;
    }

    ElapsedTime += DeltaTime;
    const float Alpha = Duration > 0.0f ? Clamp(ElapsedTime / Duration, 0.0f, 1.0f) : 1.0f;

    InOutView.ScreenEffects.bEnableGammaCorrection = true;
    InOutView.ScreenEffects.Gamma = Lerp(FromGamma, ToGamma, Alpha);

    if (Alpha >= 1.0f)
    {
        Finish();
    }

    return bFinished;
}
