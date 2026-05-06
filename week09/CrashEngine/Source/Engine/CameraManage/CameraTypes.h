#pragma once

#include "Component/CameraComponent.h"

#include <cmath>

enum class ECameraEffectType : uint8
{
    Shake,
    Fade,
    LetterBox,
    GammaCorrection,
    Vignette,
};

struct FCameraCurveKey
{
    float Time = 0.f;
    float Value = 0.f;
    float ArriveTangent = 0.f;
    float LeaveTangent = 0.f;
    bool bUseTangents = false;
};

struct FCameraFloatCurve
{
    float GetAutoTangent(size_t Index) const
    {
        if (Keys.empty())
        {
            return 0.0f;
        }

        if (Index == 0)
        {
            if (Keys.size() < 2)
            {
                return 0.0f;
            }
            const float Range = Keys[1].Time - Keys[0].Time;
            return Range > 0.0001f ? (Keys[1].Value - Keys[0].Value) / Range : 0.0f;
        }

        if (Index + 1 >= Keys.size())
        {
            const float Range = Keys[Index].Time - Keys[Index - 1].Time;
            return Range > 0.0001f ? (Keys[Index].Value - Keys[Index - 1].Value) / Range : 0.0f;
        }

        const float Range = Keys[Index + 1].Time - Keys[Index - 1].Time;
        return Range > 0.0001f ? (Keys[Index + 1].Value - Keys[Index - 1].Value) / Range : 0.0f;
    }

    float Evaluate(float Time) const
    {
        if (Keys.empty())
            return 0.f;
        if (Time<=Keys.front().Time)
            return Keys.front().Value;
        if (Time>=Keys.back().Time)
            return Keys.back().Value;
        
        for (size_t i = 1; i < Keys.size(); ++i)
        {
            const FCameraCurveKey& Prev = Keys[i - 1];
            const FCameraCurveKey& Next = Keys[i];

            if (Time <= Next.Time)
            {
                const float Range = Next.Time - Prev.Time;
                if (Range <= 0.0001f)
                {
                    return Next.Value;
                }

                const float Alpha = (Time - Prev.Time) / Range;
                const float PrevTangent = Prev.bUseTangents ? Prev.LeaveTangent : GetAutoTangent(i - 1);
                const float NextTangent = Next.bUseTangents ? Next.ArriveTangent : GetAutoTangent(i);
                const float Alpha2 = Alpha * Alpha;
                const float Alpha3 = Alpha2 * Alpha;
                const float H00 = 2.0f * Alpha3 - 3.0f * Alpha2 + 1.0f;
                const float H10 = Alpha3 - 2.0f * Alpha2 + Alpha;
                const float H01 = -2.0f * Alpha3 + 3.0f * Alpha2;
                const float H11 = Alpha3 - Alpha2;
                return H00 * Prev.Value + H10 * Range * PrevTangent + H01 * Next.Value + H11 * Range * NextTangent;
            }
        }

        return Keys.back().Value;
    }
    TArray<FCameraCurveKey> Keys;
};


struct FCameraScreenEffectInfo
{
    bool bEnableFade = false;
    FVector4 FadeColor = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    float FadeAmount = 0.0f;

    bool bEnableLetterBox = false;
    float LetterBoxAmount = 0.0f;

    bool bEnableGammaCorrection = false;
    float Gamma = 2.2f;

    bool bEnableVignette = false;
    float VignetteIntensity = 0.0f;
    float VignetteRadius = 0.75f;
    float VignetteSoftness = 0.35f;
};

struct FCameraViewInfo
{
    FVector Location;
    FRotator Rotation;
    FCameraState CameraState;
    FCameraScreenEffectInfo ScreenEffects;
};

struct FCameraShakeParams
{
    float Duration = 0.2f;
    float LocationAmplitude = 5.0f;
    float RotationAmplitude = 1.0f;
    float Frequency = 30.0f;
    
    FCameraFloatCurve LocationCurve;
    FCameraFloatCurve RotationCurve;
};

struct FCameraFadeParams
{
    FVector4 Color = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    float FromAmount = 0.0f;
    float ToAmount = 1.0f;
    float Duration = 0.25f;
    
    FCameraFloatCurve AmountCurve;
};

struct FCameraLetterBoxParams
{
    float FromAmount = 0.0f;
    float ToAmount = 0.12f;
    float Duration = 0.25f;
    float AppearRatio = 0.5f;
    float DisappearRatio = 0.5f;
    
    FCameraFloatCurve AmountCurve;
};

struct FCameraGammaCorrectionParams
{
    float FromGamma = 1.0f;
    float ToGamma = 2.2f;
    float Duration = 0.25f;
    
    FCameraFloatCurve GammaCurve;
};

struct FCameraVignetteParams
{
    float FromIntensity = 0.0f;
    float ToIntensity = 0.45f;
    float Radius = 0.75f;
    float Softness = 0.35f;
    float Duration = 0.25f;
    
    FCameraFloatCurve IntensityCurve;
};

struct FCameraEffectAsset
{
    ECameraEffectType Type = ECameraEffectType::Vignette;
    FCameraShakeParams Shake;
    FCameraFadeParams Fade;
    FCameraLetterBoxParams LetterBox;
    FCameraGammaCorrectionParams GammaCorrection;
    FCameraVignetteParams Vignette;
};
