#pragma once

#include "CameraManage/CameraTypes.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"

class AActor;
class UCameraModifier;
class UCamModifyer_CameraShake;

struct FViewTarget
{
    AActor* TargetActor = nullptr;
    UCameraComponent* POVCamera = nullptr;
};

class APlayerCameraManager : public AActor
{
public:
    DECLARE_CLASS(APlayerCameraManager, AActor)

    APlayerCameraManager();

    void SetViewTarget(UCameraComponent* NewCamera);
    void UpdateCamera(float DeltaTime);

    const FViewTarget& GetViewTarget() const { return ViewTarget; }
    const FCameraViewInfo& GetCameraViewInfoCache() const { return CameraViewInfoCache; }
    const FCameraScreenEffectInfo& GetScreenEffects() const { return BaseScreenEffects; }

    void AddCameraModifier(UCameraModifier* Modifier);
    void RemoveFinishedModifiers();

    void ResetScreenEffects();
    
    void PlayCameraShake(float Duration, float LocationAmplitude, float RotationAmplitude, float Frequency);
    void PlayCameraShake(const FCameraShakeParams& Params);
    
    void PlayCameraFade(float FromAmount, float ToAmount, float Duration, const FVector4& Color = FVector4(0.0f, 0.0f, 0.0f, 1.0f));
    void PlayCameraFade(const FCameraFadeParams& Params);
    
    void PlayCameraLetterBox(float FromAmount, float ToAmount, float Duration, float AppearRatio = 0.5f, float DisappearRatio = 0.5f);
    void PlayCameraLetterBox(const FCameraLetterBoxParams& Params);
    
    void PlayCameraGammaCorrection(float FromGamma, float ToGamma, float Duration);
    void PlayCameraGammaCorrection(const FCameraGammaCorrectionParams& Params);
    
    void PlayCameraVignette(float FromIntensity, float ToIntensity, float Duration, float Radius = 0.75f, float Softness = 0.35f);
    void PlayCameraVignette(const FCameraVignetteParams& Params);

private:
    void CommitFade(const FCameraFadeParams& Params);
    void CommitLetterBox(const FCameraLetterBoxParams& Params);
    void CommitGammaCorrection(const FCameraGammaCorrectionParams& Params);
    void CommitVignette(const FCameraVignetteParams& Params);

    FViewTarget ViewTarget;
    FCameraViewInfo CameraViewInfoCache;
    FCameraScreenEffectInfo BaseScreenEffects;

    TArray<UCameraModifier*> ModifierList;
};
