#include "Game/GameJamPlayerController.h"

#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "GameFramework/PrimitiveActors.h"

DEFINE_CLASS(AGameJamPlayerController, APlayerController)
REGISTER_FACTORY(AGameJamPlayerController)

AActor* AGameJamPlayerController::SpawnDefaultPawn()
{
	return APlayerController::SpawnDefaultPawn();
}

void AGameJamPlayerController::HandleKeyPressed(int VK)
{
	APlayerController::HandleKeyPressed(VK);
}

void AGameJamPlayerController::HandleKeyDown(int VK)
{
	APlayerController::HandleKeyDown(VK);
}

void AGameJamPlayerController::HandleKeyReleased(int VK)
{
	APlayerController::HandleKeyReleased(VK);
}

void AGameJamPlayerController::HandleMouseMove(float DeltaX, float DeltaY)
{
	if (!PossessedActor || !ViewTargetCamera)
	{
		APlayerController::HandleMouseMove(DeltaX, DeltaY);
		return;
	}

	const float YawDelta = DeltaX * LookSensitivity;
	const float PitchDelta = DeltaY * LookSensitivity;

	FVector ActorRotation = PossessedActor->GetActorRotation();
	ActorRotation.X = 0.0f;
	ActorRotation.Y = 0.0f;
	ActorRotation.Z += YawDelta;
	PossessedActor->SetActorRotation(ActorRotation);

	ViewTargetCamera->AddPitchInput(PitchDelta);
}

void AGameJamPlayerController::HandleMouseMoveAbsolute(float X, float Y)
{
	APlayerController::HandleMouseMoveAbsolute(X, Y);
}

void AGameJamPlayerController::HandleMouseButtonPressed(int VK, float X, float Y)
{
    UE_LOG("Hello!");
	APlayerController::HandleMouseButtonPressed(VK, X, Y);
}

void AGameJamPlayerController::HandleMouseButtonDown(int VK, float DeltaX, float DeltaY)
{
	APlayerController::HandleMouseButtonDown(VK, DeltaX, DeltaY);
}

void AGameJamPlayerController::HandleMouseButtonReleased(int VK, float X, float Y)
{
	APlayerController::HandleMouseButtonReleased(VK, X, Y);
}

void AGameJamPlayerController::HandleMouseDrag(int VK, float DeltaX, float DeltaY)
{
	APlayerController::HandleMouseDrag(VK, DeltaX, DeltaY);
}

void AGameJamPlayerController::HandleMouseDragEnd(int VK, float X, float Y)
{
	APlayerController::HandleMouseDragEnd(VK, X, Y);
}

void AGameJamPlayerController::HandleMouseWheel(float Notch)
{
	APlayerController::HandleMouseWheel(Notch);
}

void AGameJamPlayerController::ApplyInitialPawnTransform(ADefaultPlayerActor* Pawn, const FVector& SpawnLocation, const FVector& SpawnRotation)
{
	if (!Pawn)
	{
		return;
	}

	Pawn->SetActorLocation(SpawnLocation);

	// Game-style possession splits view rotation:
	// actor owns yaw, spring arm remains a position rig, camera owns pitch.
	Pawn->SetActorRotation(FVector(0.0f, 0.0f, SpawnRotation.Z));

	if (USpringArmComponent* SpringArm = Pawn->GetSpringArmComponent())
	{
		SpringArm->SetRelativeRotation(FVector::ZeroVector);
		SpringArm->UpdateSocketChildren();
	}

	if (UCameraComponent* Camera = Pawn->GetCameraComponent())
	{
		Camera->SetRelativeRotation(FVector(0.0f, SpawnRotation.Y, 0.0f));
	}
}

void AGameJamPlayerController::UpdatePossessedActorMovement(float DeltaTime)
{
	APlayerController::UpdatePossessedActorMovement(DeltaTime);
}

void AGameJamPlayerController::OnPossess(AActor* InActor)
{
	APlayerController::OnPossess(InActor);
}

void AGameJamPlayerController::OnUnPossess(AActor* OldActor)
{
	APlayerController::OnUnPossess(OldActor);
}
