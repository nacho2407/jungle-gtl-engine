#pragma once

#include "GameFramework/GameModeBase.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Core/CoreTypes.h"

class APawn;
class ATriggerVolumeBase;
class APoliceCar;
class FName;

// ============================================================
// AGameModeCarGame — 자동차 게임의 페이즈 진행자(Tick 기반 타이머).
//
// 흐름:
//   StartMatch       → Phase = None, RemainingMatchTime = MatchDuration(7분)
//   trigger Enter    → Phase == None 일 때만 BeginPhase(태그 매칭 페이즈)
//   trigger Exit     → 무시 (타이머가 종료의 단일 권한자)
//   Tick             → 매치/페이즈 타이머 감소 → 만료 시 EndPhase(JudgePhaseResult)
//   OnPlayerCaught   → 즉시 EndPhase(Failed)  (EscapePolice 즉시 실패 트리거)
//   EndPhase         → 결과 기록 + 클리어 마스크 갱신 → Result 페이즈(1.5s) → None
//   매치 시간 만료   → 진행 중 페이즈가 있으면 강제 판정 후 Phase = Finished
//
// "최소 1회 클리어" 비트마스크는 ClearedPhasesMask 에 누적 — 모두 켜지면 Finished.
// ============================================================
class AGameModeCarGame : public AGameModeBase
{
public:
	DECLARE_CLASS(AGameModeCarGame, AGameModeBase)

	AGameModeCarGame();
	~AGameModeCarGame() override = default;

	void StartMatch() override;
	void Tick(float DeltaTime) override;

	void OnPossessedPawnEnteredTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;
	void OnPossessedPawnExitedTrigger(ATriggerVolumeBase* Trigger, APawn* Pawn) override;

	// APoliceCar 가 player 와 충돌(잡힘)했을 때 호출. 즉시 EndPhase(Failed) 로 라우팅.
	void OnPlayerCaught(AActor* Catcher);

	void SuccessPhase();

	// --- 페이즈 시간 상수 (튜닝값 — 디자이너가 필요시 조정) ---
	static constexpr float MatchDuration       = 420.0f;  // 7분
	static constexpr float CarWashDuration     = 30.0f;
	static constexpr float CarGasDuration      = 15.0f;
	static constexpr float EscapePoliceDuration= 45.0f;
	static constexpr float DodgeMeteorDuration = 30.0f;
	static constexpr float GoalDuration        = 60.0f;  // 사실상 즉시 Success 라 timer 미사용
	static constexpr float ResultDisplayDuration = 1.5f; // 결과 표시 페이즈 길이

	// --- 페이즈 성공 임계치 ---
	static constexpr float CarGasSuccessRatio = 0.8f;     // 80% 이상 채워져야 Success

	// --- 점수 가중치 ---
	// Phase Success: BasePhaseScore + 잔여시간 비율 × PhaseTimeBonusMax (Failed 는 0)
	// Match-end:     매치 잔여 초 × MatchTimeBonusPerSec  +  잔여 HP × HealthBonusPerHP
	static constexpr int32 BasePhaseScore       = 1000;
	static constexpr int32 PhaseTimeBonusMax    = 500;
	static constexpr int32 MatchTimeBonusPerSec = 10;
	static constexpr int32 HealthBonusPerHP     = 500;

private:
	// 트리거 태그 → 페이즈 매핑
	static ECarGamePhase TagToPhase(const FName& Tag);
	static float         GetPhaseDuration(ECarGamePhase Phase);

	// 활성 페이즈 종료 — Result 페이즈로 전이.
	void BeginPhase(ECarGamePhase Target, APawn* TriggerPawn);
	void EndPhase(EPhaseResult Result);

	// 타이머 만료 시 페이즈별 성공/실패 판정.
	EPhaseResult JudgePhaseResult(ECarGamePhase Phase) const;

	// 현재 player pawn — PlayerController->GetPossessedPawn 우선, 없으면 nullptr.
	APawn* GetPlayerPawn() const;

	// 모든 페이즈 1회 클리어 시 Finished 로 전이. true 반환하면 Finished 진입.
	bool TryFinishOnAllCleared();

	// Phase=Finished 진입 직전 매치 잔여 시간 + 잔여 HP 보너스를 누적 점수에 합산.
	void ApplyMatchEndBonus();

	void SpawnPoliceCars(APawn* PlayerPawn);
	void DespawnPoliceCars();

	TArray<APoliceCar*> SpawnedPolice;
};
