#pragma once

#include "Core/CollisionTypes.h"
#include <unordered_set>
#include <vector>

class UWorld;
class UPrimitiveComponent;

// ============================================================
// FCollisionSystem — 프레임 단위 충돌 판정 시스템
//
// 컴포넌트가 직접 Register/Unregister하여 참여.
// 현재 단계: O(N^2) brute-force + 채널/응답 매트릭스 필터링
// ============================================================
class FCollisionSystem
{
public:
	void Tick(UWorld* World, float DeltaTime);

	void RegisterComponent(UPrimitiveComponent* Comp);
	void UnregisterComponent(UPrimitiveComponent* Comp);

private:
	std::vector<UPrimitiveComponent*> RegisteredComponents;

	// 이전/현재 프레임 오버랩 쌍 — Begin/End 이벤트 diff용
	std::unordered_set<FOverlapPair> PreviousOverlaps;
	std::unordered_set<FOverlapPair> CurrentOverlaps;
};
