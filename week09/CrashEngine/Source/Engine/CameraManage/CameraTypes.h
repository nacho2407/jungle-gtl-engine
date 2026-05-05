#pragma once

#include "Component/CameraComponent.h"

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
};

struct FCameraFadeParams
{
    FVector4 Color = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
    float FromAmount = 0.0f;
    float ToAmount = 1.0f;
    float Duration = 0.25f;
};

struct FCameraLetterBoxParams
{
    float FromAmount = 0.0f;
    float ToAmount = 0.12f;
    float Duration = 0.25f;
    float AppearRatio = 0.5f;
    float DisappearRatio = 0.5f;
};

struct FCameraGammaCorrectionParams
{
    float FromGamma = 1.0f;
    float ToGamma = 2.2f;
    float Duration = 0.25f;
};

struct FCameraVignetteParams
{
    float FromIntensity = 0.0f;
    float ToIntensity = 0.45f;
    float Radius = 0.75f;
    float Softness = 0.35f;
    float Duration = 0.25f;
};
