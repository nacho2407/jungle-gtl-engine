#pragma once

#include "GameFramework/AActor.h"
#include "Camera/ViewportCamera.h"

class UCameraComponent;
class ADefaultPlayerActor;
class APlayerCameraManager;
enum class ECameraBlendType;

class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	void ConfigureRuntimeCameraFromViewport(const FViewportCamera* SourceCamera);

	virtual AActor* SpawnDefaultPawn();
	void Possess(AActor* InActor);
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

	virtual void HandleKeyPressed(int VK);
	virtual void HandleKeyDown(int VK);
	virtual void HandleKeyReleased(int VK);
	virtual void HandleMouseMove(float DeltaX, float DeltaY);
	virtual void HandleMouseMoveAbsolute(float X, float Y);
	virtual void HandleMouseButtonPressed(int VK, float X, float Y);
	virtual void HandleMouseButtonDown(int VK, float DeltaX, float DeltaY);
	virtual void HandleMouseButtonReleased(int VK, float X, float Y);
	virtual void HandleMouseDrag(int VK, float DeltaX, float DeltaY);
	virtual void HandleMouseDragEnd(int VK, float X, float Y);
	virtual void HandleMouseWheel(float Notch);

	FViewportCamera* GetRuntimeCamera() { return &RuntimeCamera; }
	const FViewportCamera* GetRuntimeCamera() const { return &RuntimeCamera; }
    APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }
	AActor* GetPossessedActor() const { return PossessedActor; }
    AActor* GetViewTargetActor() const;
    UCameraComponent* GetViewTargetCamera() const;

	void SpawnPlayerCameraManager();

protected:
	UCameraComponent* FindCameraComponent(AActor* Actor) const;
	virtual AActor* FindPlayerStart() const;
	virtual AActor* FindPlacedPlayerActor() const;
	bool IsActorInCurrentWorld(const AActor* Actor) const;
	void ClearInvalidViewTarget();

	// 기본 PlayerController는 GameClient/PIE 시작 시 플레이어 Pawn과 Camera를 보장하는 bootstrap 역할만 담당합니다.
	// 게임별 이동/공격/상호작용 입력은 Lua가 Engine.API.Input(InputSystem)을 통해 직접 읽고 처리합니다.
	virtual void ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation);
	virtual void ApplyPlayerStartTransform(AActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation);

	virtual void UpdatePossessedActorMovement(float DeltaTime);
	virtual void UpdateRuntimeCameraFromViewTarget(float DeltaTime = 0.0f);
	void UpdateCameraFOVEffect(float DeltaTime, float BaseFOV);
	void ApplyCameraShakeEffect(float DeltaTime);

	virtual void OnPossess(AActor* InActor);
	virtual void OnUnPossess(AActor* OldActor);

protected:
	AActor* PossessedActor = nullptr;
    APlayerCameraManager* PlayerCameraManager = nullptr;

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

	struct FCameraShakeState
	{
		bool bActive = false;
		float Elapsed = 0.0f;
		float Duration = 0.0f;
		float Frequency = 18.0f;
		float LocationAmplitude = 0.0f;
		float RotationAmplitudeDegrees = 0.0f;
		float PhaseX = 0.0f;
		float PhaseY = 1.7f;
		float PhaseZ = 3.1f;
		float Seed = 0.0f;
	};

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

	FCameraShakeState CameraShake;
	FCameraFOVState CameraFOV;
};
