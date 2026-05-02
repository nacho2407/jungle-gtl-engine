#pragma once

#include "GameFramework/GameStateBase.h"
#include "Core/Delegate.h"

// 자동차 게임의 페이즈
enum class ECarGamePhase : uint8
{
	None = 0,
	CarWash,        // 1) 세차
	CarGas,			// 2) 주유
	EscapePolice,   // 3) 경찰차 따돌리기
	DodgeMeteor,    // 4) 운석 피하기
	Finished,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FCarGamePhaseChangedSignature, ECarGamePhase /*NewPhase*/);

// ============================================================
// AGameStateCarGame — 자동차 게임의 현재 페이즈/점수 등 데이터 보유
//
// GameMode가 페이즈를 SetPhase로 갱신하면 OnPhaseChanged가 발화된다.
// UI/Lua가 GetPhase / 델리게이트로 구독.
// ============================================================
class AGameStateCarGame : public AGameStateBase
{
public:
	DECLARE_CLASS(AGameStateCarGame, AGameStateBase)

	AGameStateCarGame() = default;
	~AGameStateCarGame() override = default;

	ECarGamePhase GetPhase() const { return CurrentPhase; }
	void SetPhase(ECarGamePhase InPhase);  // 디버그/스킵 자유 — forward-only 가드 없음

	FCarGamePhaseChangedSignature OnPhaseChanged;

	void Serialize(FArchive& Ar) override;

private:
	ECarGamePhase CurrentPhase = ECarGamePhase::None;
};
