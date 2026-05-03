#pragma once

#include "GameFramework/PlayerController.h"

class ADefaultPlayerActor;

class AGameJamPlayerController : public APlayerController
{
public:
	DECLARE_CLASS(AGameJamPlayerController, APlayerController)

	AActor* SpawnDefaultPawn() override;

	void HandleKeyPressed(int VK) override;
	void HandleKeyDown(int VK) override;
	void HandleKeyReleased(int VK) override;
	void HandleMouseMove(float DeltaX, float DeltaY) override;
	void HandleMouseMoveAbsolute(float X, float Y) override;
	void HandleMouseButtonPressed(int VK, float X, float Y) override;
	void HandleMouseButtonDown(int VK, float DeltaX, float DeltaY) override;
	void HandleMouseButtonReleased(int VK, float X, float Y) override;
	void HandleMouseDrag(int VK, float DeltaX, float DeltaY) override;
	void HandleMouseDragEnd(int VK, float X, float Y) override;
	void HandleMouseWheel(float Notch) override;

protected:
	void ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation) override;
	void UpdatePossessedActorMovement(float DeltaTime) override;
	void OnPossess(AActor* InActor) override;
	void OnUnPossess(AActor* OldActor) override;

private:
	float LookSensitivity = 0.003f;
	float MoveSpeed = 6.0f;
};
