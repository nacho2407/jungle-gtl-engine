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

	virtual void ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation);
	void AddMoveInput(float ForwardScale, float RightScale, float UpScale = 0.0f);

	virtual void UpdatePossessedActorMovement(float DeltaTime);
	virtual void UpdateRuntimeCameraFromViewTarget();

	virtual void OnPossess(AActor* InActor);
	virtual void OnUnPossess(AActor* OldActor);

protected:
	AActor* PossessedActor = nullptr;
	AActor* ViewTargetActor = nullptr;
	UCameraComponent* ViewTargetCamera = nullptr;

	FViewportCamera RuntimeCamera;
	FVector PendingMoveInput = FVector::ZeroVector;

	float MoveSpeed = 10.0f;
	float LookSensitivity = 0.15f;
};
