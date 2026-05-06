#include "CameraManage/CameraModifiers/CameraModifier_LetterBox.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier_LetterBox, UCameraModifier)

float UCameraModifier_LetterBox::Lerp(float From, float To, float Alpha) const
{
    return From + (To - From) * Alpha;
}

void UCameraModifier_LetterBox::NormalizeRatios(float& InOutAppearRatio, float& InOutDisappearRatio) const
{
    InOutAppearRatio = Clamp(InOutAppearRatio, 0.0f, 1.0f);
    InOutDisappearRatio = Clamp(InOutDisappearRatio, 0.0f, 1.0f);

    const float RatioSum = InOutAppearRatio + InOutDisappearRatio;
    if (RatioSum <= 0.0f)
    {
        InOutAppearRatio = 0.5f;
        InOutDisappearRatio = 0.5f;
    }
    else if (RatioSum > 1.0f)
    {
        InOutAppearRatio /= RatioSum;
        InOutDisappearRatio /= RatioSum;
    }
}

float UCameraModifier_LetterBox::GetCurrentAmount() const
{
    if (Duration <= 0.0f)
    {
        return FromAmount;
    }

    const float NormalizedTime = Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
    return Lerp(FromAmount, ToAmount, AmountCurve.Evaluate(NormalizedTime));
}

void UCameraModifier_LetterBox::Start(const FCameraLetterBoxParams& Params)
{
    Duration = Params.Duration;
    ElapsedTime = 0.0f;
    FromAmount = Clamp(Params.FromAmount, 0.0f, 0.5f);
    ToAmount = Clamp(Params.ToAmount, 0.0f, 0.5f);

    float AppearRatio = Params.AppearRatio;
    float DisappearRatio = Params.DisappearRatio;
    NormalizeRatios(AppearRatio, DisappearRatio);

    AppearTime = Duration * AppearRatio;
    DisappearTime = Duration * DisappearRatio;
    HoldTime = Duration - AppearTime - DisappearTime;
    if (HoldTime < 0.0f)
    {
        HoldTime = 0.0f;
    }

    AmountCurve = Params.AmountCurve;
    if (AmountCurve.Keys.empty())
    {
        const float HoldStart = AppearRatio;
        const float HoldEnd = Clamp(1.0f - DisappearRatio, HoldStart, 1.0f);
        AmountCurve.Keys = {
            {0.0f, 0.0f},
            {HoldStart, 1.0f},
            {HoldEnd, 1.0f},
            {1.0f, 0.0f}
        };
    }

    bFinished = false;
}

bool UCameraModifier_LetterBox::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (bDisabled || bFinished)
    {
        return bFinished;
    }

    ElapsedTime += DeltaTime;
    const float CurrentAmount = GetCurrentAmount();

    InOutView.ScreenEffects.bEnableLetterBox = CurrentAmount > 0.0f;
    InOutView.ScreenEffects.LetterBoxAmount = CurrentAmount;

    if (Duration <= 0.0f || ElapsedTime >= Duration)
    {
        Finish();
    }

    return bFinished;
}
