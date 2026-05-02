#pragma once

#include "GameFramework/GameModeBase.h"

class APawn;
class ATriggerVolumeBase;

// ============================================================
// AGameModeCarGame — 자동차 게임의 페이즈 전이 주체
//
// 시작 시 Phase = CarWash로 진입.
// 트리거 진입 시 TriggerTag 기반으로 현재 페이즈를 None으로 리셋한다.
// 다음 페이즈 진입은 별도 트리거/외부 로직이 처리.
// ============================================================
class AGameModeCarGame : public AGameModeBase
{
public:
	DECLARE_CLASS(AGameModeCarGame, AGameModeBase)

	AGameModeCarGame();
	~AGameModeCarGame() override = default;

	void StartMatch() override;
	void OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;
};
