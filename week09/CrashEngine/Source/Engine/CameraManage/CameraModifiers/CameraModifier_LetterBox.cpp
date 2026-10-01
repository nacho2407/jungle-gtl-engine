#include "CameraManage/CameraModifiers/CameraModifier_LetterBox.h"

#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier_LetterBox, UCameraModifier)

float UCameraModifier_LetterBox::Lerp(float From, float To, float Alpha) const
{
    return From + (To - From) * Alpha;
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

    AmountCurve = Params.AmountCurve;
    if (AmountCurve.Keys.empty())
    {
        AmountCurve.Keys = {{0.0f, 0.0f}, {1.0f, 1.0f}};
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
