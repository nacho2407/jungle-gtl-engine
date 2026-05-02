#pragma once

#include "GameFramework/PlayerController.h"

// ============================================================
// APlayerControllerCarGame — 자동차 게임 전용 PlayerController
//
// 베이스 PlayerController를 그대로 상속. 향후 차량 입력 라우팅,
// HUD 표시, 카메라 전환 등 게임 고유 로직이 들어갈 자리.
// AGameModeCarGame이 PlayerControllerClass로 지정.
// ============================================================
class APlayerControllerCarGame : public APlayerController
{
public:
	DECLARE_CLASS(APlayerControllerCarGame, APlayerController)

	APlayerControllerCarGame() = default;
	~APlayerControllerCarGame() override = default;
};
