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

	void AddForce(UPrimitiveComponent* Comp, const FVector& Force) override;
	void AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation) override;
	void AddTorque(UPrimitiveComponent* Comp, const FVector& Torque) override;

	FVector GetLinearVelocity(UPrimitiveComponent* Comp) const override;
	void SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;
	FVector GetAngularVelocity(UPrimitiveComponent* Comp) const override;
	void SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;

	void SetMass(UPrimitiveComponent* Comp, float Mass) override;
	float GetMass(UPrimitiveComponent* Comp) const override;
	void SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset) override;
	FVector GetCenterOfMass(UPrimitiveComponent* Comp) const override;

	bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit, const AActor* IgnoreActor = nullptr) const override;

private:
	UWorld* World = nullptr;
	std::vector<UPrimitiveComponent*> RegisteredComponents;

	// 물리 시뮬레이션 상태
	struct FBodyState
	{
		FVector Velocity = { 0, 0, 0 };
		FVector AngularVelocity = { 0, 0, 0 };
		FVector AccumulatedForce = { 0, 0, 0 };
		FVector AccumulatedTorque = { 0, 0, 0 };
		float Mass = 1.0f;
		FVector CenterOfMassLocal = { 0, 0, 0 }; // 컴포넌트 local 좌표계 offset
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
