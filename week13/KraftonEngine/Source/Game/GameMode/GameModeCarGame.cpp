#include "Game/GameMode/GameModeCarGame.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Game/PlayerController/PlayerControllerCarGame.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "Object/FName.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(AGameModeCarGame, AGameModeBase)

AGameModeCarGame::AGameModeCarGame()
{
	// 클래스 ID는 생성자에서 확정 — World가 GameMode를 spawn한 직후 BeginPlay/StartMatch
	// 시점에 이 값으로 GameState/PlayerController가 만들어진다.
	GameStateClass = AGameStateCarGame::StaticClass();
	PlayerControllerClass = APlayerControllerCarGame::StaticClass();
}

void AGameModeCarGame::StartMatch()
{
	Super::StartMatch();

	if (auto* GS = Cast<AGameStateCarGame>(GetGameState()))
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] StartMatch — Phase = None");
	}
}

void AGameModeCarGame::OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* /*Pawn*/)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS || !Trigger) return;

	const FName Tag = Trigger->GetTriggerTag();
	const ECarGamePhase Cur = GS->GetPhase();

	if (Tag == FName("CarWash") && Cur != ECarGamePhase::CarWash)
	{
		GS->SetPhase(ECarGamePhase::CarWash);
		UE_LOG("[CarGame] CarWash — Phase");
	}
	else if (Tag == FName("CarGas") && Cur != ECarGamePhase::CarGas)
	{
		GS->SetPhase(ECarGamePhase::CarGas);
		UE_LOG("[CarGame] CarGas — Phase");
	}
	else if (Tag == FName("EscapePolice") && Cur != ECarGamePhase::EscapePolice)
	{
		GS->SetPhase(ECarGamePhase::EscapePolice);
		UE_LOG("[CarGame] EscapePolice — Phase");
	}
	else if (Tag == FName("DodgeMeteor") && Cur != ECarGamePhase::DodgeMeteor)
	{
		GS->SetPhase(ECarGamePhase::DodgeMeteor);
		UE_LOG("[CarGame] DodgeMeteor — Phase");
	}
}

void AGameModeCarGame::OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* /*Pawn*/)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS || !Trigger) return;

	const FName Tag = Trigger->GetTriggerTag();
	const ECarGamePhase Cur = GS->GetPhase();

	// 이탈한 트리거가 "현재 활성 페이즈"의 영역일 때만 None으로 리셋.
	// 이미 다른 페이즈로 전환된 상태라면 무시 (잔여 EndOverlap이 와도 안전).
	if (Tag == FName("CarWash") && Cur == ECarGamePhase::CarWash)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] CarWash exit — Phase = None");
	}
	else if (Tag == FName("EscapePolice") && Cur == ECarGamePhase::EscapePolice)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] EscapePolice exit — Phase = None");
	}
	else if (Tag == FName("DodgeMeteor") && Cur == ECarGamePhase::DodgeMeteor)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] DodgeMeteor exit — Phase = None");
	}
}
