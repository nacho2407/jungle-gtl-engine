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

void AGameStateCarGame::SetScore(int32 V)
{
	if (V < 0) V = 0;
	if (CurrentScore == V) return;
	CurrentScore = V;
	OnScoreChanged.Broadcast(CurrentScore);
}

void AGameStateCarGame::ResetScore()
{
	CurrentScore = 0;
	NextScoreEventId = 1;
	ScoreEvents.clear();
	OnScoreChanged.Broadcast(CurrentScore);
}

void AGameStateCarGame::AddScore(int32 Delta, EScoreCategory Category, const FString& Reason, ECarGamePhase SourcePhase)
{
	if (Delta == 0) return;

	const int32 OldScore = CurrentScore;
	int32 NewScore = CurrentScore + Delta;
	if (NewScore < 0) NewScore = 0;

	const int32 ActualDelta = NewScore - OldScore;
	if (ActualDelta == 0) return;

	CurrentScore = NewScore;

	FScoreEvent Event;
	Event.SequenceId = NextScoreEventId++;
	Event.Amount = ActualDelta;
	Event.TotalScoreAfter = CurrentScore;
	Event.Category = Category;
	Event.SourcePhase = SourcePhase;
	Event.Reason = Reason;
	Event.RemainingMatchTime = RemainingMatchTime;
	Event.RemainingPhaseTime = RemainingPhaseTime;
	ScoreEvents.push_back(Event);

	OnScoreChanged.Broadcast(CurrentScore);
}

const FScoreEvent* AGameStateCarGame::GetScoreEvent(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(ScoreEvents.size()))
	{
		return nullptr;
	}
	return &ScoreEvents[static_cast<size_t>(Index)];
}

const FScoreEvent* AGameStateCarGame::GetLatestScoreEvent() const
{
	if (ScoreEvents.empty())
	{
		return nullptr;
	}
	return &ScoreEvents.back();
}

void AGameStateCarGame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << CurrentPhase;
}
