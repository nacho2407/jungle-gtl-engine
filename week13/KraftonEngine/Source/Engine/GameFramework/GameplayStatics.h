#pragma once

#include "Core/CoreTypes.h"

class AActor;
class UWorld;
class FName;

// ============================================================
// FGameplayStatics — 게임플레이 측에서 자주 쓰는 검색/유틸 모음 (UE 의 UGameplayStatics 대응).
//
// 범용 액터 쿼리는 여기에 모아두고, 게임-specific (예: AGameModeCarGame 한정 헬퍼) 은
// 해당 GameMode/State 에 둔다. 모든 함수는 static — 인스턴스화 불필요.
// ============================================================
class FGameplayStatics
{
public:
	// Tag 검색 — AActor::HasTag(InTag) 가 true 인 액터들. 큰 월드에서 매 frame 호출은
	// 권장 안 함 (선형 스캔). 결과 캐싱 또는 BeginPlay 1회 lookup 후 보관 패턴 추천.
	static AActor* FindFirstActorByTag(const UWorld* World, const FName& Tag);
	static TArray<AActor*> FindActorsByTag(const UWorld* World, const FName& Tag);
};
