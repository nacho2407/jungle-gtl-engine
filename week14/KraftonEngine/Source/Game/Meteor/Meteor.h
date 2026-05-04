#pragma once

#include "GameFramework/AActor.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

// ============================================================
// AMeteor — DodgeMeteor 페이즈에서 spawn되는 운석 액터
//
// 컴포넌트 트리:
//   RootComponent: USphereComponent (충돌 + SimulatePhysics)
//     └─ UStaticMeshComponent (시각만)
//
// 동작:
//   - SimulatePhysics=true → 중력으로 떨어짐
//   - 차량과 충돌 시 ACarPawn::TakeDamage 호출 후 자기 destroy
//   - Lifetime 만료 시 자기 destroy (안 닿거나 lost된 운석 안전망)
//
// MeteorSpawner.lua가 World.SpawnActor("AMeteor")로 생성.
// ============================================================
class AMeteor : public AActor
{
public:
	DECLARE_CLASS(AMeteor, AActor)

	AMeteor() = default;
	~AMeteor() override = default;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	// 코드 spawn 시 호출 — 직렬화 경로에선 PostDuplicate가 캐시 다시 잡음
	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/meteor/meteor.obj");
	void PostDuplicate() override;

	USphereComponent* GetCollisionSphere() const { return CollisionSphere; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	// Spawn 직후 호출하면 초기 launch 속도 부여. CollisionSphere 의 PhysX body 가
	// BeginPlay 에서 register 된 뒤에만 의미 있음 — World::SpawnActor 가 BeginPlay 까지
	// 끝낸 상태로 반환하므로 호출자(MeteorSpawner.lua) 시점에 안전.
	void SetLaunchVelocity(const FVector& Vel);

	float Lifetime = 4.0f;        // 초 — 만료 시 자기 destroy. spawn rate × Lifetime 이
	                              // MAX_CONCURRENT 보다 작아야 saturation 안 걸림.
	float DamagePerHit = 10.0f;   // 차량과 충돌 시 가하는 데미지
	static constexpr float UnderworldZ = -20.0f;  // Z 가 이 값 미만이면 off-map 추락 → 즉시 destroy

private:
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ResolveCachedComponents();

	USphereComponent* CollisionSphere = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	float ElapsedTime = 0.0f;
};
