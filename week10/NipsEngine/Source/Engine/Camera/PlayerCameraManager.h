#pragma once
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"
#include "Camera/CameraModifier_CameraShake.h"


class UCameraComponent;
class UCameraModifier;
class APlayerController;

class APlayerCameraManager : public AActor
{
public:
    ~APlayerCameraManager() override;

    void BeginPlay() override;
    void Tick(float DeltaTime) override;

    // ===== Core API =====
    void SetViewTarget(AActor* NewTarget);
    const FMinimalViewInfo& GetCameraView() const;

    // ===== Fade =====
    void StartFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color);
    void StopFade();

    // ===== Modifier =====
    void AddModifier(UCameraModifier* Modifier);
    void RemoveModifier(UCameraModifier* Modifier);

    void StopAllCameraShakes(bool bImmediately = false);

	const FColor& GetFadeColor() const { return Fade.Color; }
    float GetFadeAlpha() const { return Fade.CurrentAlpha; }

	void InitializeFor(APlayerController* PC);
    virtual APlayerController* GetOwningPlayerController() const { return PCOwner; }

	AActor* GetViewTargetActor() const { return ViewTarget.Target; }
    UCameraComponent* GetViewTargetCamera() const { return ViewTarget.CameraComp; }

private:
    struct FViewTarget
    {
        AActor* Target = nullptr;
        UCameraComponent* CameraComp = nullptr;
		
		// 카메라 시점 상태 스냅샷
        FMinimalViewInfo POV;
    };

    FViewTarget ViewTarget;

    struct FCameraTransition
    {
        FMinimalViewInfo From;
        FMinimalViewInfo To;

        float TotalTime = 0.f;
        float RemainingTime = 0.f;

        bool bActive = false;
    };

    FCameraTransition Transition;

    struct FCameraFade
    {
        FColor Color = FColor::Black();

        float FromAlpha = 0.f;
        float ToAlpha = 0.f;

        float Duration = 0.f;
        float Elapsed = 0.f;

        float CurrentAlpha = 0.f;

        bool bActive = false;
    };

    FCameraFade Fade;

    TArray<UCameraModifier*> ModifierList;

private:
    void UpdateCamera(float DeltaTime);
    void UpdateViewTarget(float DeltaTime);
    void ComputeCamera(float DeltaTime, FMinimalViewInfo& OutView);
    void ApplyModifiers(float DeltaTime, FMinimalViewInfo& InOutView);
    UCameraModifier_CameraShake* GetOrCreateCameraShakeModifier();

    void UpdateTransition(float DeltaTime, FMinimalViewInfo& InOutView);
    void UpdateFade(float DeltaTime);

private:
	APlayerController* PCOwner = nullptr;
    FMinimalViewInfo CachedView;
    UCameraModifier_CameraShake* CacheCameraShakeMod = nullptr;

public:
    template <typename PatternType>
    UCameraShakeBase* StartCameraShake(float Scale = 1.0f, float DurationOverride = 0.0f)
    {
        UCameraModifier_CameraShake* Modifier = GetOrCreateCameraShakeModifier();
        if (!Modifier)
        {
            return nullptr;
        }

        return Modifier->AddCameraShake<PatternType>(Scale, DurationOverride);
    }

    UCameraShakeBase* StartCameraShake(
        const FString& PatternTypeName,
        float Scale = 1.0f,
        float DurationOverride = 0.0f)
    {
        UCameraModifier_CameraShake* Modifier = GetOrCreateCameraShakeModifier();
        if (!Modifier)
        {
            return nullptr;
        }

        return Modifier->AddCameraShakeByPatternTypeName(
            PatternTypeName,
            Scale,
            DurationOverride);
    }

    template <typename ModifierType>
    ModifierType* FindCameraModifier()
    {
        for (UCameraModifier* Modifier : ModifierList)
        {
            if (ModifierType* Casted = Cast<ModifierType>(Modifier))
            {
                return Casted;
            }
        }

        return nullptr;
    }
};