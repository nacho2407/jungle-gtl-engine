#pragma once

#include "GameFramework/AActor.h"

class AGameStateBase;
class ATriggerVolumeBase;
class APawn;
class APlayerController;
class UClass;

// ============================================================
// AGameModeBase — 게임 룰/페이즈 전이의 주체
//
// World당 하나 존재. WorldType이 Editor가 아닐 때만 World가 BeginPlay 시점에 spawn한다.
// GameState를 보유/생성하며, TriggerVolume으로부터 받은 이벤트를
// 페이즈 전이 로직으로 변환한다.
// ============================================================
class AGameModeBase : public AActor
{
public:
	DECLARE_CLASS(AGameModeBase, AActor)

	AGameModeBase();
	~AGameModeBase() override = default;

	// AActor
	void BeginPlay() override;
	void EndPlay() override;

	// --- Match flow ---
	// StartMatch는 모든 액터의 BeginPlay가 끝난 뒤 World가 호출한다.
	// 페이즈 초기화·플레이어 Possess 등 "게임 시작" 시점 작업을 여기에 둔다.
	virtual void StartMatch();
	virtual void EndMatch();

	// --- TriggerVolume 콜백 ---
	// Possessed Pawn이 진입/이탈한 경우에만 호출된다 (TriggerVolumeBase가 필터링).
	// 서브클래스가 페이즈 전이를 처리한다.
	virtual void OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) {}
	virtual void OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) {}

	// --- Accessors ---
	AGameStateBase* GetGameState() const { return GameState; }
	UClass* GetGameStateClass() const { return GameStateClass; }
	APlayerController* GetPlayerController() const { return PlayerController; }
	UClass* GetPlayerControllerClass() const { return PlayerControllerClass; }

protected:
	// StartMatch가 호출 — 씬에서 bAutoPossessPlayer = true인 첫 APawn을 찾아 Possess 한다.
	void AutoPossessFirstPawn();

	// 서브클래스 생성자에서 지정 — null이면 AGameStateBase가 spawn된다.
	UClass* GameStateClass = nullptr;
	UClass* PlayerControllerClass = nullptr;

	// GameMode가 BeginPlay/StartMatch에서 spawn하여 소유.
	AGameStateBase* GameState = nullptr;
	APlayerController* PlayerController = nullptr;
};
