#include "CameraManage/APlayerCameraManager.h"

#include "CameraManage/CameraModifiers/CameraModifier.h"
#include "CameraManage/CameraModifiers/CameraModifier_Fade.h"
#include "CameraManage/CameraModifiers/CameraModifier_GammaCorrection.h"
#include "CameraManage/CameraModifiers/CameraModifier_LetterBox.h"
#include "CameraManage/CameraModifiers/CameraModifier_Vignette.h"
#include "CameraManage/CameraModifiers/UCamModifyer_CameraShake.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

APlayerCameraManager::APlayerCameraManager()
{
    SetActorTickEnabled(false);
}

void APlayerCameraManager::SetViewTarget(UCameraComponent* NewCamera)
{
    ViewTarget.POVCamera = NewCamera;
    ViewTarget.TargetActor = NewCamera ? NewCamera->GetOwner() : nullptr;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    UCameraComponent* Camera = ViewTarget.POVCamera;
    if (!Camera)
    {
        return;
    }

    CameraViewInfoCache.Location = Camera->GetWorldLocation();
    CameraViewInfoCache.Rotation = Camera->GetWorldMatrix().ToRotator();
    CameraViewInfoCache.CameraState = Camera->GetCameraState();
    CameraViewInfoCache.ScreenEffects = BaseScreenEffects;

    std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* Lhs, const UCameraModifier* Rhs)
    {
        if (!Lhs)
            return false;
        if (!Rhs)
            return true;
        return Lhs->GetPriority() < Rhs->GetPriority();
    });

    for (UCameraModifier* Modifier : ModifierList)
    {
        if (!Modifier || Modifier->IsDisabled())
        {
            continue;
        }

        Modifier->ModifyCamera(DeltaTime, CameraViewInfoCache);
    }

    RemoveFinishedModifiers();
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* Modifier)
{
    if (!Modifier)
    {
        return;
    }

    ModifierList.push_back(Modifier);
}

void APlayerCameraManager::RemoveFinishedModifiers()
{
    ModifierList.erase(
        std::remove_if(ModifierList.begin(), ModifierList.end(), [](UCameraModifier* Modifier)
        {
            if (!Modifier || Modifier->IsFinished())
            {
                if (Modifier)
                {
                    UObjectManager::Get().DestroyObject(Modifier);
                }
                return true;
            }
            return false;
        }),
        ModifierList.end());
}

void APlayerCameraManager::ResetScreenEffects()
{
    BaseScreenEffects = FCameraScreenEffectInfo{};
}

void APlayerCameraManager::PlayCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency)
{
    FCameraShakeParams Params;
    Params.Duration = Duration;
    Params.LocationAmplitude = LocationAmplitude;
    Params.RotationAmplitude = RotationAmplitude;
    Params.Frequency = Frequency;
    PlayCameraShake(Params);
}

void APlayerCameraManager::PlayCameraShake(const FCameraShakeParams& Params)
{
    UCamModifyer_CameraShake* Shake = UObjectManager::Get().CreateObject<UCamModifyer_CameraShake>();
    if (!Shake)
    {
        return;
    }

    Shake->Start(Params);
    AddCameraModifier(Shake);
}

void APlayerCameraManager::PlayCameraFade(float FromAmount, float ToAmount, float Duration, const FVector4& Color)
{
    FCameraFadeParams Params;
    Params.Color = Color;
    Params.FromAmount = FromAmount;
    Params.ToAmount = ToAmount;
    Params.Duration = Duration;
    PlayCameraFade(Params);
}

void APlayerCameraManager::PlayCameraFade(const FCameraFadeParams& Params)
{
    UCameraModifier_Fade* Modifier = UObjectManager::Get().CreateObject<UCameraModifier_Fade>();
    if (!Modifier)
    {
        return;
    }

    Modifier->Start(Params);
    CommitFade(Params);
    AddCameraModifier(Modifier);
}

void APlayerCameraManager::PlayCameraLetterBox(float FromAmount, float ToAmount, float Duration, float AppearRatio, float DisappearRatio)
{
    FCameraLetterBoxParams Params;
    Params.FromAmount = FromAmount;
    Params.ToAmount = ToAmount;
    Params.Duration = Duration;
    Params.AppearRatio = AppearRatio;
    Params.DisappearRatio = DisappearRatio;
    PlayCameraLetterBox(Params);
}

void APlayerCameraManager::PlayCameraLetterBox(const FCameraLetterBoxParams& Params)
{
    UCameraModifier_LetterBox* Modifier = UObjectManager::Get().CreateObject<UCameraModifier_LetterBox>();
    if (!Modifier)
    {
        return;
    }

    Modifier->Start(Params);
    CommitLetterBox(Params);
    AddCameraModifier(Modifier);
}

void APlayerCameraManager::PlayCameraGammaCorrection(float FromGamma, float ToGamma, float Duration)
{
    FCameraGammaCorrectionParams Params;
    Params.FromGamma = FromGamma;
    Params.ToGamma = ToGamma;
    Params.Duration = Duration;
    PlayCameraGammaCorrection(Params);
}

void APlayerCameraManager::PlayCameraGammaCorrection(const FCameraGammaCorrectionParams& Params)
{
    UCameraModifier_GammaCorrection* Modifier = UObjectManager::Get().CreateObject<UCameraModifier_GammaCorrection>();
    if (!Modifier)
    {
        return;
    }

    Modifier->Start(Params);
    CommitGammaCorrection(Params);
    AddCameraModifier(Modifier);
}

void APlayerCameraManager::PlayCameraVignette(float FromIntensity, float ToIntensity, float Duration, float Radius, float Softness)
{
    FCameraVignetteParams Params;
    Params.FromIntensity = FromIntensity;
    Params.ToIntensity = ToIntensity;
    Params.Duration = Duration;
    Params.Radius = Radius;
    Params.Softness = Softness;
    PlayCameraVignette(Params);
}

void APlayerCameraManager::PlayCameraVignette(const FCameraVignetteParams& Params)
{
    UCameraModifier_Vignette* Modifier = UObjectManager::Get().CreateObject<UCameraModifier_Vignette>();
    if (!Modifier)
    {
        return;
    }

    Modifier->Start(Params);
    CommitVignette(Params);
    AddCameraModifier(Modifier);
}

void APlayerCameraManager::CommitFade(const FCameraFadeParams& Params)
{
    BaseScreenEffects.bEnableFade = Params.ToAmount > 0.0f;
    BaseScreenEffects.FadeColor = Params.Color;
    BaseScreenEffects.FadeAmount = Clamp(Params.ToAmount, 0.0f, 1.0f);
}

void APlayerCameraManager::CommitLetterBox(const FCameraLetterBoxParams& Params)
{
    BaseScreenEffects.bEnableLetterBox = Params.FromAmount > 0.0f;
    BaseScreenEffects.LetterBoxAmount = Clamp(Params.FromAmount, 0.0f, 0.5f);
}

void APlayerCameraManager::CommitGammaCorrection(const FCameraGammaCorrectionParams& Params)
{
    BaseScreenEffects.bEnableGammaCorrection = Params.ToGamma != 1.0f;
    BaseScreenEffects.Gamma = Clamp(Params.ToGamma, 0.01f, 8.0f);
}

void APlayerCameraManager::CommitVignette(const FCameraVignetteParams& Params)
{
    BaseScreenEffects.bEnableVignette = Params.ToIntensity > 0.0f;
    BaseScreenEffects.VignetteIntensity = Clamp(Params.ToIntensity, 0.0f, 1.0f);
    BaseScreenEffects.VignetteRadius = Clamp(Params.Radius, 0.0f, 2.0f);
    BaseScreenEffects.VignetteSoftness = Clamp(Params.Softness, 0.001f, 2.0f);
}
