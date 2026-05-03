#pragma once

#include "GameFramework/GameModeBase.h"
#include "Core/CoreTypes.h"

class APawn;
class ATriggerVolumeBase;
class APoliceCar;

// ============================================================
// AGameModeCarGame — 자동차 게임의 페이즈 전이 주체
//
// 시작 시 Phase = None.
// 트리거 진입 시 TriggerTag(CarWash/EscapePolice/DodgeMeteor)에 해당하는
// 페이즈로 진입하고, 트리거 이탈 시 그 페이즈가 활성 상태이면 None으로 리셋한다.
//
// EscapePolice 페이즈 진입 시 APoliceCar 를 N대 동적 spawn — 페이즈 이탈 또는
// player가 잡혔을 때(OnPlayerCaught) 모두 destroy.
// ============================================================
class AGameModeCarGame : public AGameModeBase
{
public:
	DECLARE_CLASS(AGameModeCarGame, AGameModeBase)

	AGameModeCarGame();
	~AGameModeCarGame() override = default;

	void StartMatch() override;
	void OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;
	void OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;

	// APoliceCar 가 player와 충돌(잡힘)했을 때 호출. 페이즈 Failed 로 전환 + 추적 정리.
	void OnPlayerCaught(AActor* Catcher);

private:
	void SpawnPoliceCars(APawn* PlayerPawn);
	void DespawnPoliceCars();

	TArray<APoliceCar*> SpawnedPolice;
};
