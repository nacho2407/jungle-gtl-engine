#include "Game/GameState/GameStateCarGame.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(AGameStateCarGame, AGameStateBase)

void AGameStateCarGame::SetPhase(ECarGamePhase InPhase)
{
	if (CurrentPhase == InPhase) return;
	CurrentPhase = InPhase;
	OnPhaseChanged.Broadcast(CurrentPhase);
}

void AGameStateCarGame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CurrentPhase;
}
