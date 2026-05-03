#pragma once

#include "GameFramework/AActor.h"
#include "Camera/ViewportCamera.h"

class UCameraComponent;
class ADefaultPlayerActor;

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

	void SetViewTarget(AActor* InActor);
	void SetViewTargetCamera(UCameraComponent* InCamera);
	void NotifyObservedActorDestroyed(AActor* DestroyedActor);
	void SetCursorVisible(bool bVisible);
	bool IsCursorVisible() const;
	void SetCursorLocked(bool bLocked);
	bool IsCursorLocked() const;
	void SetInputModeGameOnly();
	void SetInputModeUIOnly();
	void SetInputModeGameAndUI();

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
	AActor* GetPossessedActor() const { return PossessedActor; }
	AActor* GetViewTargetActor() const { return ViewTargetActor; }
	UCameraComponent* GetViewTargetCamera() const { return ViewTargetCamera; }

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
	virtual void UpdateRuntimeCameraFromViewTarget();

	virtual void OnPossess(AActor* InActor);
	virtual void OnUnPossess(AActor* OldActor);

protected:
	AActor* PossessedActor = nullptr;
	AActor* ViewTargetActor = nullptr;
	UCameraComponent* ViewTargetCamera = nullptr;

	FViewportCamera RuntimeCamera;
};
