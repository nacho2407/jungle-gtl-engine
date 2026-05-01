#pragma once

#include "Physics/IPhysicsScene.h"
#include "Core/CollisionTypes.h"
#include "Math/Vector.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>

// ============================================================
// FNativePhysicsScene — 기존 hand-written 충돌 시스템 래핑
//
// O(N²) brute-force + CollisionMath + 채널/응답 필터링.
// IPhysicsScene 인터페이스를 통해 교체 가능.
// ============================================================
class FNativePhysicsScene : public IPhysicsScene
{
public:
	void Initialize(UWorld* InWorld) override;
	void Shutdown() override;

	void RegisterComponent(UPrimitiveComponent* Comp) override;
	void UnregisterComponent(UPrimitiveComponent* Comp) override;

	void Tick(float DeltaTime) override;

private:
	UWorld* World = nullptr;
	std::vector<UPrimitiveComponent*> RegisteredComponents;

	// 물리 시뮬레이션 상태
	struct FBodyState
	{
		FVector Velocity = { 0, 0, 0 };
	};
	std::unordered_map<UPrimitiveComponent*, FBodyState> BodyStates;

	static constexpr float GravityZ = -9.81f;

	// 이전/현재 프레임 오버랩 쌍 — Begin/End 이벤트 diff용
	std::unordered_set<FOverlapPair> PreviousOverlaps;
	std::unordered_set<FOverlapPair> CurrentOverlaps;

	// Block 쌍 추적 — Hit 이벤트는 첫 접촉 시에만 발화
	std::unordered_set<FOverlapPair> PreviousBlockPairs;
	std::unordered_set<FOverlapPair> CurrentBlockPairs;
};
