#pragma once

#include "Core/CoreTypes.h"
#include <functional>

class AActor;
class UWorld;
struct FVector;

// ============================================================
// FActorPlacementRegistry — Editor 의 "Place Actor" 컨텍스트 메뉴를 위한
// 액터 프리셋 레지스트리.
//
// Engine 일반 항목(Cube/Sphere/Light/Collider 등)은 Editor 가 빌트인으로 처리하고,
// 게임 모듈은 자기 액터(예: ACarPawn)를 RegisterEntry 로 추가만 한다. 이렇게 하면
// Editor 가 Game 헤더를 직접 include 할 필요 없이 데이터만 주고받게 된다.
//
// 등록은 모듈 init 시 1회 (RegisterGameActorPlacements 등). 등록 순서가 메뉴 표시
// 순서. 같은 라벨을 두 번 등록하면 마지막이 살아있게 됨 (덮어쓰기 X — 중복 push).
// ============================================================
class FActorPlacementRegistry
{
public:
	using FSpawnFn = std::function<AActor*(UWorld*, const FVector&)>;

	struct FEntry
	{
		FString  Label;
		FSpawnFn SpawnFn;
	};

	static FActorPlacementRegistry& Get();

	void RegisterEntry(const FString& Label, FSpawnFn SpawnFn);
	const TArray<FEntry>& GetEntries() const { return Entries; }

	// 디버그/툴용 — 같은 모듈 재등록 시 깨끗이 시작하고 싶을 때.
	void Clear() { Entries.clear(); }

private:
	FActorPlacementRegistry() = default;
	TArray<FEntry> Entries;
};
