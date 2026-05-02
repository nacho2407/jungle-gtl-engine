#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

IMPLEMENT_CLASS(APlayerController, AActor)

void APlayerController::Possess(APawn* Pawn)
{
	if (!Pawn || PossessedPawn == Pawn) return;

	if (PossessedPawn)
	{
		UnPossess();
	}

	PossessedPawn = Pawn;
	Pawn->PossessedBy(this);
}

void APlayerController::UnPossess()
{
	if (!PossessedPawn) return;

	APawn* OldPawn = PossessedPawn;
	PossessedPawn = nullptr;
	OldPawn->UnPossessed();
}
