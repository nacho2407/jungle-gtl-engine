#pragma once

#include "GameFramework/GameModeBase.h"

#include "Source/Game/GameMode/GameModeIntro.generated.h"
// ============================================================
// AGameModeIntro — 인트로 씬 전용 GameMode.
//
// 게임플레이 로직 없음 — 인트로 씬 (Asset/Scene/Intro.Scene) 의 환경 위에서
// IntroWidget 이 떠 있다가 사용자가 "게임 시작" 을 누르면 Map.Scene 으로 transition
// 하는 흐름의 placeholder. AGameModeBase 가 PlayerController spawn / AutoPossess
// 까지 처리하므로 본 클래스에 추가 로직은 없다 (필요 시 인트로 카메라 연출 등을
// 여기에 얹는다).
// ============================================================
UCLASS()
class AGameModeIntro : public AGameModeBase
{
public:
	GENERATED_BODY()
	AGameModeIntro() = default;
	~AGameModeIntro() override = default;
};
