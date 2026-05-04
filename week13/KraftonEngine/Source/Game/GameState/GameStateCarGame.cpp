#include "Game/GameState/GameStateCarGame.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(AGameStateCarGame, AGameStateBase)

void AGameStateCarGame::SetPhase(ECarGamePhase InPhase)
{
	if (CurrentPhase == InPhase) return;
	CurrentPhase = InPhase;
	OnPhaseChanged.Broadcast(CurrentPhase);
}

void AGameStateCarGame::SetHealth(int32 V)
{
	if (V < 0) V = 0;
	if (V > MaxHealth) V = MaxHealth;
	if (CurrentHealth == V) return;
	CurrentHealth = V;
	OnHealthChanged.Broadcast(CurrentHealth);
}

void AGameStateCarGame::LoseHealth(int32 Amount)
{
	if (Amount <= 0) return;
	SetHealth(CurrentHealth - Amount);
}

void AGameStateCarGame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CurrentPhase;
}
