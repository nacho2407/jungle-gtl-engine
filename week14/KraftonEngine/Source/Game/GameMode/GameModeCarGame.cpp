#include "Game/GameMode/GameModeCarGame.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Game/PlayerController/PlayerControllerCarGame.h"
#include "Game/Pawn/PoliceCar.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Core/Log.h"

#include <cmath>
#include <cstdlib>

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

void AGameModeCarGame::OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn)
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
		SpawnPoliceCars(Pawn);
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
	else if (Tag == FName("CarGas") && Cur == ECarGamePhase::CarGas)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] CarGas exit — Phase = None");
	}
	else if (Tag == FName("EscapePolice") && Cur == ECarGamePhase::EscapePolice)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] EscapePolice exit — Phase = None");
		DespawnPoliceCars();
	}
	else if (Tag == FName("DodgeMeteor") && Cur == ECarGamePhase::DodgeMeteor)
	{
		GS->SetPhase(ECarGamePhase::None);
		UE_LOG("[CarGame] DodgeMeteor exit — Phase = None");
	}
}

void AGameModeCarGame::OnPlayerCaught(AActor* /*Catcher*/)
{
	auto* GS = Cast<AGameStateCarGame>(GetGameState());
	if (!GS) return;

	if (GS->GetPhase() == ECarGamePhase::Failed) return;  // 중복 호출 가드

	GS->SetPhase(ECarGamePhase::Failed);
	UE_LOG("[CarGame] Player caught — Phase = Failed");

	DespawnPoliceCars();
}

void AGameModeCarGame::SpawnPoliceCars(APawn* PlayerPawn)
{
	if (!PlayerPawn) return;
	UWorld* W = GetWorld();
	if (!W) return;

	// 기존에 남아있는 경찰차 정리 후 새로 spawn (이중 진입 방어)
	DespawnPoliceCars();

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	constexpr int32 SpawnCount = 3;
	constexpr float MinRadius = 30.0f;
	constexpr float MaxRadius = 45.0f;

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		// SpawnActor → AddActor 경로에서 즉시 BeginPlay 가 호출되며, APoliceCar 가
		// 그 안에서 lazy 로 InitDefaultPoliceComponents 를 수행한다. 여기서는 spawn 직후
		// target 과 위치만 주입하면 된다.
		auto* Police = W->SpawnActor<APoliceCar>();
		if (!Police) continue;

		Police->SetTarget(PlayerPawn);

		// player 주변 랜덤 각도 + 30~45m 거리 — 처음부터 가까이 붙진 않도록
		const float RandT = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		const float Angle = (static_cast<float>(i) / SpawnCount + RandT * 0.1f) * 6.28318f; // 균등 분산
		const float Radius = MinRadius + (MaxRadius - MinRadius) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
		FVector Offset(std::cos(Angle) * Radius, std::sin(Angle) * Radius, 0.0f);
		Police->SetActorLocation(PlayerLoc + Offset);

		SpawnedPolice.push_back(Police);

		UE_LOG("[Police] spawn idx=%d at (%.1f,%.1f,%.1f)", i,
			(PlayerLoc + Offset).X, (PlayerLoc + Offset).Y, (PlayerLoc + Offset).Z);
	}
}

void AGameModeCarGame::DespawnPoliceCars()
{
	// FPhysXSimulationCallback 가 hit/overlap 이벤트를 큐잉했다가 PhysicsScene::Tick
	// 말미(simulate/fetchResults 외부)에서 dispatch 하므로, 이 함수가 trigger 콜백
	// 경로에서 호출돼도 World->DestroyActor 를 직접 부르는 것이 안전하다.
	UWorld* W = GetWorld();
	if (!W) { SpawnedPolice.clear(); return; }

	for (APoliceCar* P : SpawnedPolice)
	{
		if (P) W->DestroyActor(P);
	}
	SpawnedPolice.clear();
}
