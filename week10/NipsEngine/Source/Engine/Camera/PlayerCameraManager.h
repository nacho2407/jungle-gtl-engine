#pragma once
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"


class UCameraComponent;
class UCameraModifier;
class APlayerController;

class APlayerCameraManager : public AActor
{
public:
    void BeginPlay() override;
    void Tick(float DeltaTime) override;

    // ===== Core API =====
    void SetViewTarget(AActor* NewTarget);
    void GetCameraView(float DeltaTime, FMinimalViewInfo& OutView);

    // ===== Fade =====
    void StartFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color);
    void StopFade();

    // ===== Modifier =====
    void AddModifier(UCameraModifier* Modifier);
    void RemoveModifier(UCameraModifier* Modifier);

	const FColor& GetFadeColor() const { return Fade.Color; }
    float GetFadeAlpha() const { return Fade.CurrentAlpha; }

	void InitializeFor(APlayerController* PC);
    virtual APlayerController* GetOwningPlayerController() const { return PCOwner; }

private:
    struct FViewTarget
    {
        AActor* Target = nullptr;
        UCameraComponent* CameraComp = nullptr;

        FVector Location;
        FRotator Rotation;
        float FOV = 90.f;
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
    void UpdateViewTarget(float DeltaTime);
    void ComputeCamera(float DeltaTime, FMinimalViewInfo& OutView);
    void ApplyModifiers(float DeltaTime, FMinimalViewInfo& InOutView);

    void UpdateTransition(float DeltaTime, FMinimalViewInfo& InOutView);
    void UpdateFade(float DeltaTime);

private:
	APlayerController* PCOwner = nullptr;
};