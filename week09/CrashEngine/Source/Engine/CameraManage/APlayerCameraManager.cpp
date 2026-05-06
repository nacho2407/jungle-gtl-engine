#include "CameraManage/APlayerCameraManager.h"

#include "CameraManage/CameraModifiers/CameraModifier.h"
#include "CameraManage/CameraModifiers/CameraModifier_Fade.h"
#include "CameraManage/CameraModifiers/CameraModifier_GammaCorrection.h"
#include "CameraManage/CameraModifiers/CameraModifier_LetterBox.h"
#include "CameraManage/CameraModifiers/CameraModifier_Vignette.h"
#include "CameraManage/CameraModifiers/UCamModifyer_CameraShake.h"
#include "CameraManage/CameraEffectAssetManager.h"
#include "GameFramework/AActor.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"

#include <algorithm>
#include <cctype>
#include <cmath>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

namespace
{
FString NormalizeBlendTypeName(const FString& BlendType)
{
    FString Result;
    Result.reserve(BlendType.size());

    for (char Ch : BlendType)
    {
        if (Ch == '_' || Ch == '-' || Ch == ' ')
        {
            continue;
        }

        Result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(Ch))));
    }

    return Result;
}

ECameraTransitionBlendType ParseCameraTransitionBlendType(const FString& BlendType)
{
    const FString NormalizedBlendType = NormalizeBlendTypeName(BlendType);

    if (NormalizedBlendType == "linear")
    {
        return ECameraTransitionBlendType::Linear;
    }
    if (NormalizedBlendType == "easein")
    {
        return ECameraTransitionBlendType::EaseIn;
    }
    if (NormalizedBlendType == "easeout")
    {
        return ECameraTransitionBlendType::EaseOut;
    }

    return ECameraTransitionBlendType::Cubic;
}

FRotator MakeLookAtRotation(const FVector& Location, const FVector& Target)
{
    FVector Direction = Target - Location;
    if (Direction.LengthSquared() <= 0.0001f)
    {
        return FRotator{};
    }

    Direction = Direction.Normalized();

    constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

    FRotator Rotation = {};
    Rotation.Pitch = -asinf(Direction.Z) * Rad2Deg;
    Rotation.Roll = 0.0f;

    if (fabsf(Direction.Z) < 0.999f)
    {
        Rotation.Yaw = atan2f(Direction.Y, Direction.X) * Rad2Deg;
    }

    return Rotation;
}

FCameraViewInfo MakeCameraViewInfo(const FCameraViewInfo& CurrentViewInfo, const FVector& Location, const FVector& Target, float FovDegrees, const FCameraScreenEffectInfo& ScreenEffects)
{
    FCameraViewInfo NewCameraViewInfo = CurrentViewInfo;
    NewCameraViewInfo.Location = Location;
    NewCameraViewInfo.Rotation = MakeLookAtRotation(Location, Target);
    NewCameraViewInfo.CameraState.FOV = FovDegrees * DEG_TO_RAD;
    NewCameraViewInfo.ScreenEffects = ScreenEffects;
    return NewCameraViewInfo;
}
} // namespace

APlayerCameraManager::APlayerCameraManager()
{
    SetActorTickEnabled(false);
}

void APlayerCameraManager::SetViewTarget(AActor* NewActor)
{
    if (!NewActor)
    {
        ViewTarget = FViewTarget{};
        bHasCameraViewOverride = false;
        return;
    }

    bHasCameraViewOverride = false;
    ViewTarget.TargetActor = NewActor;
    ViewTarget.POVCamera = nullptr;

    for (UActorComponent* Comp : NewActor->GetComponents())
    {
        if (UCameraComponent* Camera = Cast<UCameraComponent>(Comp))
        {
            ViewTarget.POVCamera = Camera;
            break;
        }
    }
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
    if (!ViewTarget.TargetActor)
    {
        return;
    }

    CameraViewInfoCache.ScreenEffects = BaseScreenEffects;
    if (ViewTarget.POVCamera)
    {
        CameraViewInfoCache.Location = ViewTarget.POVCamera->GetWorldLocation();
        CameraViewInfoCache.Rotation = ViewTarget.POVCamera->GetWorldMatrix().ToRotator();
        CameraViewInfoCache.CameraState = ViewTarget.POVCamera->GetCameraState();
    }
    else
    {
        CameraViewInfoCache.Location = ViewTarget.TargetActor->GetActorLocation();
        CameraViewInfoCache.Rotation = ViewTarget.TargetActor->GetActorRotation();
        CameraViewInfoCache.CameraState = FCameraState{};
    }
    if (bHasCameraViewOverride && !TransitionState.bActive)
    {
        CameraViewInfoCache = CameraViewOverride;
        CameraViewInfoCache.ScreenEffects = BaseScreenEffects;
    }

    std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* Lhs, const UCameraModifier* Rhs)
              {
        if (!Lhs)
            return false;
        if (!Rhs)
            return true;
        return Lhs->GetPriority() < Rhs->GetPriority(); });

    UpdateTransition(DeltaTime);

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
            return false; }),
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

void APlayerCameraManager::PlayCameraLetterBox(float FromAmount, float ToAmount, float Duration)
{
    FCameraLetterBoxParams Params;
    Params.FromAmount = FromAmount;
    Params.ToAmount = ToAmount;
    Params.Duration = Duration;
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

void APlayerCameraManager::PlayCameraEffect(const FCameraEffectAsset& Asset)
{
    switch (Asset.Type)
    {
    case ECameraEffectType::Shake:
        PlayCameraShake(Asset.Shake);
        break;
    case ECameraEffectType::Fade:
        PlayCameraFade(Asset.Fade);
        break;
    case ECameraEffectType::LetterBox:
        PlayCameraLetterBox(Asset.LetterBox);
        break;
    case ECameraEffectType::GammaCorrection:
        PlayCameraGammaCorrection(Asset.GammaCorrection);
        break;
    case ECameraEffectType::Vignette:
        PlayCameraVignette(Asset.Vignette);
        break;
    default:
        break;
    }
}

bool APlayerCameraManager::PlayCameraEffectAsset(const FString& AssetPath)
{
    FCameraEffectAsset Asset;
    if (!FCameraEffectAssetManager::GetOrLoad(AssetPath, Asset))
    {
        return false;
    }

    PlayCameraEffect(Asset);
    return true;
}

void APlayerCameraManager::CommitFade(const FCameraFadeParams& Params)
{
    BaseScreenEffects.bEnableFade = Params.ToAmount > 0.0f;
    BaseScreenEffects.FadeColor = Params.Color;
    BaseScreenEffects.FadeAmount = Clamp(Params.ToAmount, 0.0f, 1.0f);
}

void APlayerCameraManager::CommitLetterBox(const FCameraLetterBoxParams& Params)
{
    BaseScreenEffects.bEnableLetterBox = Params.ToAmount > 0.0f;
    BaseScreenEffects.LetterBoxAmount = Clamp(Params.ToAmount, 0.0f, 0.5f);
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

void APlayerCameraManager::SetViewTargetWithBlend(AActor* NewActor, const FCameraTransitionParams& Params)
{
    if (!NewActor)
    {
        return;
    }

    FCameraViewInfo NewCameraViewInfo = {};

    OldCameraViewInfoCache = CameraViewInfoCache;
    SetViewTarget(NewActor);
    bHasCameraViewOverride = false;
    if (ViewTarget.POVCamera != nullptr)
    {
        NewCameraViewInfo.Location = ViewTarget.POVCamera->GetWorldLocation();
        NewCameraViewInfo.Rotation = ViewTarget.POVCamera->GetWorldMatrix().ToRotator();
        NewCameraViewInfo.CameraState = ViewTarget.POVCamera->GetCameraState();
    }
    else
    {
        NewCameraViewInfo.Location = NewActor->GetActorLocation();
        NewCameraViewInfo.Rotation = NewActor->GetActorRotation();
        NewCameraViewInfo.CameraState = FCameraState{};
    }
    NewCameraViewInfo.ScreenEffects = BaseScreenEffects;

    TransitionState.bActive = true;
    TransitionState.FromView = CameraViewInfoCache;
    TransitionState.ToView = NewCameraViewInfo;
    TransitionState.Duration = Params.Duration;
    TransitionState.ElapsedTime = 0;
    TransitionState.BlendType = Params.BlendType;
}

void APlayerCameraManager::SetCameraTransitionToTarget(AActor* NewActor, float Duration, const FString& BlendType)
{
    FCameraTransitionParams Params;
    Params.Duration = Duration;
    Params.BlendType = ParseCameraTransitionBlendType(BlendType);

    SetViewTargetWithBlend(NewActor, Params);
}

void APlayerCameraManager::SetCameraTransitionToView(const FVector& Location, const FVector& Target, float FovDegrees, float Duration, const FString& BlendType)
{
    FCameraViewInfo NewCameraViewInfo = MakeCameraViewInfo(CameraViewInfoCache, Location, Target, FovDegrees, BaseScreenEffects);
    CameraViewOverride = NewCameraViewInfo;
    bHasCameraViewOverride = true;

    if (TransitionState.bActive)
    {
        TransitionState.ToView = NewCameraViewInfo;
        return;
    }

    if (Duration <= 0.0f)
    {
        CameraViewInfoCache = NewCameraViewInfo;
        TransitionState.bActive = false;
        return;
    }

    OldCameraViewInfoCache = CameraViewInfoCache;

    TransitionState.bActive = true;
    TransitionState.FromView = CameraViewInfoCache;
    TransitionState.ToView = NewCameraViewInfo;
    TransitionState.Duration = Duration;
    TransitionState.ElapsedTime = 0.0f;
    TransitionState.BlendType = ParseCameraTransitionBlendType(BlendType);
}

void APlayerCameraManager::SetCameraViewImmediate(const FVector& Location, const FVector& Target, float FovDegrees)
{
    CameraViewOverride = MakeCameraViewInfo(CameraViewInfoCache, Location, Target, FovDegrees, BaseScreenEffects);
    CameraViewInfoCache = CameraViewOverride;
    bHasCameraViewOverride = true;
    TransitionState.bActive = false;
}

void APlayerCameraManager::UpdateTransition(float DeltaTime)
{
    if (!TransitionState.bActive)
    {
        return;
    }

    TransitionState.ElapsedTime += DeltaTime;

    const float NormalizedTime = TransitionState.Duration > 0.0f
                                     ? Clamp(TransitionState.ElapsedTime / TransitionState.Duration, 0.0f, 1.0f)
                                     : 1.0f;

    const float T = CameraTransition::EvaluateBlendAlpha(NormalizedTime, TransitionState.BlendType);

    auto LerpFloat = [](float From, float To, float Alpha)
    {
        return From + (To - From) * Alpha;
    };

    auto LerpAngleDegrees = [&](float From, float To, float Alpha)
    {
        float Delta = To - From;

        while (Delta > 180.0f)
        {
            Delta -= 360.0f;
        }

        while (Delta < -180.0f)
        {
            Delta += 360.0f;
        }

        return From + Delta * Alpha;
    };

    const FCameraViewInfo& FromView = OldCameraViewInfoCache;
    const FCameraViewInfo& ToView = TransitionState.ToView;

    CameraViewInfoCache.Location = FromView.Location + (ToView.Location - FromView.Location) * T;

    CameraViewInfoCache.Rotation.Pitch = LerpAngleDegrees(FromView.Rotation.Pitch, ToView.Rotation.Pitch, T);
    CameraViewInfoCache.Rotation.Yaw = LerpAngleDegrees(FromView.Rotation.Yaw, ToView.Rotation.Yaw, T);
    CameraViewInfoCache.Rotation.Roll = LerpAngleDegrees(FromView.Rotation.Roll, ToView.Rotation.Roll, T);

    CameraViewInfoCache.CameraState.FOV = LerpFloat(FromView.CameraState.FOV, ToView.CameraState.FOV, T);
    CameraViewInfoCache.CameraState.AspectRatio = LerpFloat(FromView.CameraState.AspectRatio, ToView.CameraState.AspectRatio, T);
    CameraViewInfoCache.CameraState.NearZ = LerpFloat(FromView.CameraState.NearZ, ToView.CameraState.NearZ, T);
    CameraViewInfoCache.CameraState.FarZ = LerpFloat(FromView.CameraState.FarZ, ToView.CameraState.FarZ, T);
    CameraViewInfoCache.CameraState.OrthoWidth = LerpFloat(FromView.CameraState.OrthoWidth, ToView.CameraState.OrthoWidth, T);

    CameraViewInfoCache.CameraState.bIsOrthogonal = T < 1.0f
                                                        ? FromView.CameraState.bIsOrthogonal
                                                        : ToView.CameraState.bIsOrthogonal;
    if (NormalizedTime >= 1.0f)
    {
        CameraViewInfoCache.Location = ToView.Location;
        CameraViewInfoCache.Rotation = ToView.Rotation;
        CameraViewInfoCache.CameraState = ToView.CameraState;

        TransitionState.bActive = false;
    }
}
