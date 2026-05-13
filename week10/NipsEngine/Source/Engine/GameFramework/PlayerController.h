#pragma once

#include "GameFramework/AActor.h"
#include "GameFramework/Pawn.h"
#include "Camera/ViewportCamera.h"
#include "Engine/Input/GameplayInputTypes.h"

class UCameraComponent;
class APlayerCameraManager;
enum class ECameraBlendType;

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	void ConfigureRuntimeCameraFromViewport(const FViewportCamera* SourceCamera);

	void Possess(APawn* InPawn);
	void UnPossess();

	void NotifyObservedActorDestroyed(AActor* DestroyedActor);
	void SetCursorVisible(bool bVisible);
	bool IsCursorVisible() const;
	void SetCursorLocked(bool bLocked);
	bool IsCursorLocked() const;
	void SetMouseCapture(bool bCaptured);
	void ReleaseMouseCapture();
	bool IsMouseCaptured() const;
	void SetInputModeGameOnly();
	void SetInputModeUIOnly();
	void SetInputModeGameAndUI();
	void PlayCameraShake(float Intensity, float Duration);
	void PlayCameraShakeDetailed(float LocationAmplitude, float RotationAmplitudeDegrees, float Frequency, float Duration);
	void LerpCameraFOVDegrees(float TargetFOVDegrees, float Duration);
	void ResetCameraFOV(float Duration);
	void StopCameraEffects();
	void SetViewTargetWithBlend(AActor* InActor, float BlendTime, ECameraBlendType BlendType);
	void SetDefaultViewTargetBlend(float BlendTime, ECameraBlendType BlendType);
	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, const FColor& Color = FColor::Black());
	void StopCameraFade();
	void SetCameraVignette(float Intensity, float Radius = 0.75f, float Smoothness = 0.35f, const FColor& Color = FColor::Black());
	void ClearCameraVignette();
	void StartCameraLetterbox(float TargetAspect = 16.0f / 9.0f, float Duration = 0.0f);
	void StopCameraLetterbox(float Duration = 0.0f);
	void SetCameraLetterbox(float TargetAspect = 16.0f / 9.0f);
	void ClearCameraLetterbox();

	void ProcessInputSnapshot(const FGameplayInputSnapshot& Snapshot);
	void ReceiveInputSnapshot(const FGameplayInputSnapshot& Snapshot);
	const FGameplayInputSnapshot& GetInputSnapshot() const { return CurrentInputSnapshot; }
	const FInputActionState* FindInputAction(const FString& ActionName) const;

	FViewportCamera* GetRuntimeCamera() { return &RuntimeCamera; }
	const FViewportCamera* GetRuntimeCamera() const { return &RuntimeCamera; }
    APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }
	APawn* GetPawn() const { return PossessedPawn; }
	AActor* GetPossessedActor() const { return PossessedPawn; }
    AActor* GetViewTargetActor() const;
    UCameraComponent* GetViewTargetCamera() const;

	void SpawnPlayerCameraManager();

protected:
	UCameraComponent* FindCameraComponent(AActor* Actor) const;
	virtual AActor* FindPlayerStart() const;
	bool IsActorInCurrentWorld(const AActor* Actor) const;
	void ClearInvalidViewTarget();

	// 기본 PlayerController는 GameInputBridge/GameEngine에서 만든 gameplay action snapshot을 Pawn 쪽으로 전달합니다.
	void DispatchInputActionsToPawn();
	virtual void UpdateRuntimeCameraFromViewTarget(float DeltaTime = 0.0f);
	void UpdateCameraFOVEffect(float DeltaTime, float BaseFOV);

	virtual void OnPossess(APawn* InPawn);
	virtual void OnUnPossess(APawn* OldPawn);

protected:
	APawn* PossessedPawn = nullptr;
    APlayerCameraManager* PlayerCameraManager = nullptr;
	FGameplayInputSnapshot CurrentInputSnapshot;

    struct FPendingCameraFade
    {
        float FromAlpha = 0.0f;
        float ToAlpha = 0.0f;
        float Duration = 0.0f;
        FColor Color = FColor::Black();
        bool bPending = false;
    };

    FPendingCameraFade PendingCameraFade;

	FViewportCamera RuntimeCamera;

	struct FCameraFOVState
	{
		bool bActive = false;
		bool bResetToBase = false;
		bool bOverrideActive = false;
		float Elapsed = 0.0f;
		float Duration = 0.0f;
		float StartFOV = 0.0f;
		float TargetFOV = 0.0f;
		float OverrideFOV = 0.0f;
	};

	FCameraFOVState CameraFOV;
};
