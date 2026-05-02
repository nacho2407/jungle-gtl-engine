#pragma once

#include "GameFramework/AActor.h"

class APawn;

// ============================================================
// APlayerController — 플레이어의 의도(Possess/입력)를 Pawn에 전달
//
// Pawn은 "조종 가능한 액터"이고, PlayerController는 "조종자".
// World당 (지금은) 1개만 spawn되며 GameMode가 spawn/관리.
// ============================================================
class APlayerController : public AActor
{
public:
	DECLARE_CLASS(APlayerController, AActor)

	APlayerController() = default;
	~APlayerController() override = default;

	// Pawn을 점유한다. 이미 다른 Pawn을 점유 중이면 먼저 해제.
	void Possess(APawn* Pawn);
	void UnPossess();

	APawn* GetPossessedPawn() const { return PossessedPawn; }

private:
	APawn* PossessedPawn = nullptr;  // 직렬화 제외
};
