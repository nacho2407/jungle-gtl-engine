#include "GameFramework/Pawn.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(APawn, AActor)

void APawn::PossessedBy(APlayerController* PC)
{
	Controller = PC;
}

void APawn::UnPossessed()
{
	Controller = nullptr;
}

void APawn::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << bAutoPossessPlayer;
}
