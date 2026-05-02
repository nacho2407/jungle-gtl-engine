#pragma once

#include "GameFramework/GameModeBase.h"

class APawn;
class ATriggerVolumeBase;

// ============================================================
// AGameModeCarGame — 자동차 게임의 페이즈 전이 주체
//
// 시작 시 Phase = None.
// 트리거 진입 시 TriggerTag(CarWash/EscapePolice/DodgeMeteor)에 해당하는
// 페이즈로 진입하고, 트리거 이탈 시 그 페이즈가 활성 상태이면 None으로 리셋한다.
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
};
