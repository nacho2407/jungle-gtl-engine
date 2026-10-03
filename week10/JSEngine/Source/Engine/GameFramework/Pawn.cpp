#include "GameFramework/Pawn.h"

#include "GameFramework/PlayerController.h"

DEFINE_CLASS(APawn, AActor)
REGISTER_FACTORY(APawn)

void APawn::PossessedBy(APlayerController* NewController)
{
	Controller = NewController;
}

void APawn::UnPossessed()
{
	Controller = nullptr;
}

void APawn::OnInputAction(const FInputActionState&)
{
}
