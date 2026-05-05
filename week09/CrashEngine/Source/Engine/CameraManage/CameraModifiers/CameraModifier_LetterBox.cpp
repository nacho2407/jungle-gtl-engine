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

    if (ElapsedTime <= AppearTime)
    {
        const float Alpha = AppearTime > 0.0f ? Clamp(ElapsedTime / AppearTime, 0.0f, 1.0f) : 1.0f;
        return Lerp(FromAmount, ToAmount, Alpha);
    }

    const float HoldEndTime = AppearTime + HoldTime;
    if (ElapsedTime <= HoldEndTime)
    {
        return ToAmount;
    }

    const float DisappearElapsedTime = ElapsedTime - HoldEndTime;
    const float Alpha = DisappearTime > 0.0f ? Clamp(DisappearElapsedTime / DisappearTime, 0.0f, 1.0f) : 1.0f;
    return Lerp(ToAmount, FromAmount, Alpha);
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
