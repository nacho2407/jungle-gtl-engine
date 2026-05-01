#pragma once

#include "GameFramework/PlayerController.h"

class AGameJamPlayerController : public APlayerController
{
public:
	DECLARE_CLASS(AGameJamPlayerController, APlayerController)

	AActor* SpawnDefaultPawn() override;
	void HandleKeyPressed(int VK) override;
	void HandleKeyDown(int VK) override;
	void HandleKeyReleased(int VK) override;
	void HandleMouseMove(float DeltaX, float DeltaY) override;

protected:
	void ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation) override;
	void UpdatePossessedActorMovement(float DeltaTime) override;
	void OnPossess(AActor* InActor) override;
	void OnUnPossess(AActor* OldActor) override;
};
