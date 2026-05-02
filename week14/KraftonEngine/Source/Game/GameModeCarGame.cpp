#include "Game/GameModeCarGame.h"
#include "Game/GameStateCarGame.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "Object/FName.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(AGameModeCarGame, AGameModeBase)

AGameModeCarGame::AGameModeCarGame()
{
	GameStateClass = AGameStateCarGame::StaticClass();
}

void AGameModeCarGame::StartMatch()
{
	Super::StartMatch();

	if (auto* GS = Cast<AGameStateCarGame>(GetGameState()))
	{
		GS->SetPhase(ECarGamePhase::CarWash);
		UE_LOG("[CarGame] StartMatch — Phase = CarWash");
	}
}

void AGameModeCarGame::OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* /*Pawn*/)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS || !Trigger) return;

	const FName Tag = Trigger->GetTriggerTag();
	const ECarGamePhase Cur = GS->GetPhase();

	// 각 태그는 해당 페이즈일 때만 페이즈를 None으로 리셋한다.
	// 다음 페이즈 진입은 외부(다른 트리거/스크립트)가 결정.
	if (Tag == FName("CarWashEnd") && Cur == ECarGamePhase::CarWash)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] CarWashEnd — Phase reset to None");
	}
	else if (Tag == FName("EscapePoliceEnd") && Cur == ECarGamePhase::EscapePolice)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] EscapePoliceEnd — Phase reset to None");
	}
	else if (Tag == FName("DodgeMeteorEnd") && Cur == ECarGamePhase::DodgeMeteor)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] DodgeMeteorEnd — Phase reset to None");
	}
}
