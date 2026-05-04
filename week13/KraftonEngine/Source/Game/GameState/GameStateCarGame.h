#pragma once

#include "GameFramework/GameStateBase.h"
#include "Core/Delegate.h"
#include "Core/CoreTypes.h"

// 자동차 게임의 페이즈 — 게임 진행 상태 (활성 페이즈 / 결과 표시 / 종료).
// 페이즈 결과(성공/실패)는 EPhaseResult 로 분리해 LastPhaseResult 에 저장한다.
enum class ECarGamePhase : uint8
{
	None = 0,
	CarWash,        // 1) 세차
	CarGas,			// 2) 주유
	EscapePolice,   // 3) 경찰차 따돌리기
	DodgeMeteor,    // 4) 운석 피하기
	Result,         // 직전 페이즈 결과(성공/실패) UI 표시용 짧은 페이즈
	Finished,       // 매치 종료 (시간 만료 또는 모든 페이즈 1회 클리어)
};

// 페이즈 종료 결과 — Result 페이즈 동안 LastPhaseResult 에 저장돼 UI/Lua 가 폴링.
enum class EPhaseResult : uint8
{
	None = 0,
	Success,
	Failed,
};

// 매치 종료(Phase=Finished) 시점의 최종 결과 — Win / Lose 분기.
//   Win  : 모든 페이즈 1회 클리어 후 도달 (또는 매치 시간 만료 시 모두 클리어 상태)
//   Lose : HP 0 도달 또는 매치 시간 만료 시 미클리어 페이즈 잔존
enum class EFinishOutcome : uint8
{
	None = 0,
	Win,
	Lose,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FCarGamePhaseChangedSignature, ECarGamePhase /*NewPhase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FCarGameHealthChangedSignature, int32 /*NewHealth*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FCarGameScoreChangedSignature,  int32 /*NewScore*/);

// ============================================================
// AGameStateCarGame — 자동차 게임의 현재 페이즈/타이머/누적 결과 데이터 보유
//
// GameMode 가 SetPhase / 타이머 / Mask 를 갱신. UI/Lua 는 GetPhase /
// GetRemainingMatchTime / GetRemainingPhaseTime 등으로 폴링.
// ============================================================
class AGameStateCarGame : public AGameStateBase
{
public:
	DECLARE_CLASS(AGameStateCarGame, AGameStateBase)

	AGameStateCarGame() = default;
	~AGameStateCarGame() override = default;

	ECarGamePhase GetPhase() const { return CurrentPhase; }
	void SetPhase(ECarGamePhase InPhase);  // 디버그/스킵 자유 — forward-only 가드 없음
	ECarGamePhase GetQuestPhase() const { return CurrentQuestPhase; }
	void SetQuestPhase(ECarGamePhase InPhase) { CurrentQuestPhase = InPhase; }

	// --- Timers (read by UI / Lua) ---
	float GetRemainingMatchTime() const { return RemainingMatchTime; }
	float GetRemainingPhaseTime() const { return RemainingPhaseTime; }

	// --- 결과 / 진행도 누적 ---
	ECarGamePhase GetLastEndedPhase() const { return LastEndedPhase; }
	EPhaseResult  GetLastPhaseResult() const { return LastPhaseResult; }
	uint32        GetClearedPhasesMask() const { return ClearedPhasesMask; }

	// --- HP (페이즈 실패 시 1씩 차감, 0 도달 시 GameMode 가 Finished 로 전이) ---
	int32 GetHealth() const { return CurrentHealth; }
	int32 GetMaxHealth() const { return MaxHealth; }
	void  SetHealth(int32 V);
	void  LoseHealth(int32 Amount = 1);

	// --- Finish outcome (Phase=Finished 시점 최종 결과) ---
	EFinishOutcome GetFinishOutcome() const { return FinishOutcome; }
	void           SetFinishOutcome(EFinishOutcome V) { FinishOutcome = V; }

	// --- Score (페이즈 성공 보너스 + 매치 종료 보너스 누적) ---
	int32 GetScore() const { return CurrentScore; }
	void  SetScore(int32 V);
	void  AddScore(int32 Delta);

	// --- GameMode 전용 setter (private 으로 두지 않은 이유: friend 회피 + 단일 호출자 가정) ---
	void SetRemainingMatchTime(float V) { RemainingMatchTime = V; }
	void SetRemainingPhaseTime(float V) { RemainingPhaseTime = V; }
	void SetLastEndedPhase(ECarGamePhase P) { LastEndedPhase = P; }
	void SetLastPhaseResult(EPhaseResult R) { LastPhaseResult = R; }
	void MarkPhaseCleared(ECarGamePhase P) { ClearedPhasesMask |= (1u << static_cast<uint32>(P)); }

	FCarGamePhaseChangedSignature OnPhaseChanged;
	FCarGameHealthChangedSignature OnHealthChanged;
	FCarGameScoreChangedSignature  OnScoreChanged;

	void Serialize(FArchive& Ar) override;

private:
	ECarGamePhase CurrentPhase = ECarGamePhase::None;
	ECarGamePhase CurrentQuestPhase = ECarGamePhase::None;

	float RemainingMatchTime = 0.0f;   // 매치 전체 카운트다운 (GameMode 가 StartMatch 에서 초기화)
	float RemainingPhaseTime = 0.0f;   // 활성 페이즈 카운트다운

	ECarGamePhase LastEndedPhase  = ECarGamePhase::None; // Result 페이즈 동안 UI 가 참조
	EPhaseResult  LastPhaseResult = EPhaseResult::None;

	uint32 ClearedPhasesMask = 0;      // bit per ECarGamePhase — Success 시 set

	static constexpr int32 DefaultMaxHealth = 3;
	int32 MaxHealth     = DefaultMaxHealth;
	int32 CurrentHealth = DefaultMaxHealth;

	EFinishOutcome FinishOutcome = EFinishOutcome::None;

	int32 CurrentScore = 0;
};
