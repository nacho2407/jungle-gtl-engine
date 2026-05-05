#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Object/Object.h"
#include "GameFramework/CameraTypes.h"

class UCameraComponent;
class UCameraShakeBase;
class AActor;
class UClass;

class UCameraManager : public UObject
{
public:
	DECLARE_CLASS(UCameraManager, UObject)
	UCameraManager() = default;
	~UCameraManager() override = default;

	// ─── Camera 등록 / Active / Possess ────────────────────────────
	void RegisterCamera(UCameraComponent* Camera);
	void UnregisterCamera(UCameraComponent* Camera);

	void AutoPossessDefaultCamera();
	bool ToggleActiveCameraForActor(const FString& ActorName);
	bool ToggleActiveCameraForActor(const AActor* Actor);

	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }
	void SetActiveCamera(UCameraComponent* NewCamera) { ActiveCamera = NewCamera; }

	UCameraComponent* GetPossessedCamera() const { return PossessedCamera; }
	void Possess(UCameraComponent* NewCamera) { PossessedCamera = NewCamera; }

	// ─── View Target ──────────────────────────────────────────────
	// PlayerController::SetViewTargetWithBlend 가 위임. 현재 view target → New 로 전환.
	// UE: APlayerCameraManager::SetViewTarget
	virtual void SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	AActor* GetViewTarget() const { return ViewTarget; }
	AActor* GetPendingViewTarget() const { return PendingViewTarget; }

	// ─── Camera Shake ─────────────────────────────────────────────
	// UE: APlayerCameraManager::StartCameraShake
	virtual UCameraShakeBase* StartCameraShake(
		UClass* ShakeClass,
		float Scale = 1.0f,
		ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal,
		FRotator UserPlaySpaceRot = FRotator());

	// 템플릿 헬퍼 — TSubclassOf 대용. 호출부: CameraMgr->StartCameraShake<UMyShake>(1.0f);
	template<typename T>
	T* StartCameraShake(
		float Scale = 1.0f,
		ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal,
		FRotator UserPlaySpaceRot = FRotator())
	{
		return static_cast<T*>(StartCameraShake(T::StaticClass(), Scale, PlaySpace, UserPlaySpaceRot));
	}

	// UE: StopCameraShake
	virtual void StopCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately = true);
	virtual void StopAllCameraShakes(bool bImmediately = true);
	virtual void StopAllInstancesOfCameraShake(UClass* ShakeClass, bool bImmediately = true);

	// ─── Camera Fade ──────────────────────────────────────────────
	// UE: APlayerCameraManager::StartCameraFade
	virtual void StartCameraFade(
		float FromAlpha,
		float ToAlpha,
		float Duration,
		FLinearColor Color,
		bool bShouldFadeAudio = false,
		bool bHoldWhenFinished = false);

	virtual void StopCameraFade();

	// UE: SetManualCameraFade — 외부에서 매 프레임 수동으로 알파를 갱신할 때.
	virtual void SetManualCameraFade(float InFadeAmount, FLinearColor Color, bool bInFadeAudio);

	// 현재 fade 알파 (0..1) — RenderPass(B) 가 PostProcess 에 전달.
	float GetFadeAmount() const { return FadeAmount; }
	FLinearColor GetFadeColor() const { return FadeColor; }
	bool IsFadeEnabled() const { return bEnableFading; }

	// ─── Tick ─────────────────────────────────────────────────────
	// World::Tick 에서 매 프레임 호출. Shake / Fade / ViewTarget blend 갱신.
	virtual void UpdateCamera(float DeltaTime);

private:
	TSet<UCameraComponent*> RegisteredCameras;
	TArray<UCameraComponent*> RegisteredCameraOrder;

	UCameraComponent* ActiveCamera = nullptr;		// Rendering Camera
	UCameraComponent* PossessedCamera = nullptr;	// Input/Control Camera

	// View Target
	AActor* ViewTarget = nullptr;
	AActor* PendingViewTarget = nullptr;
	FViewTargetTransitionParams BlendParams;
	float BlendTimeRemaining = 0.0f;

	// Active shakes — 매니저가 소유. UpdateCamera 에서 IsFinished() 시 제거.
	TArray<UCameraShakeBase*> ActiveShakes;

	// Fade 상태
	bool bEnableFading = false;
	bool bHoldFadeWhenFinished = false;
	bool bFadeAudio = false;
	float FadeAmount = 0.0f;
	float FadeAlphaFrom = 0.0f;
	float FadeAlphaTo = 0.0f;
	float FadeDuration = 0.0f;
	float FadeTimeRemaining = 0.0f;
	FLinearColor FadeColor = FLinearColor::Black();
};
