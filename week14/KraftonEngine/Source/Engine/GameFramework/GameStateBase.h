#pragma once

#include "GameFramework/AActor.h"

// ============================================================
// AGameStateBase — 게임의 현재 상태(데이터) 보유
//
// GameMode가 World당 하나 spawn하며, 게임플레이 코드/UI/Lua가
// 여기서 점수·페이즈·남은 시간 등을 읽는다.
// 베이스 자체는 비어 있고, 구체 게임이 서브클래스에서 필드를 정의한다.
// ============================================================
class AGameStateBase : public AActor
{
public:
	DECLARE_CLASS(AGameStateBase, AActor)

	AGameStateBase() = default;
	~AGameStateBase() override = default;
};
