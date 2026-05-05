#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

class UWorld;
class AActor;
class UPrimitiveComponent;
struct FHitResult;

// 물리 백엔드 선택
enum class EPhysicsBackend : uint8
{
	Native,		// Hand-written collision math (O(N²) brute-force)
	PhysX,		// NVIDIA PhysX 4.1
};

// ============================================================
// IPhysicsScene — 물리 시스템 어댑터 인터페이스
//
// World가 소유하며, PrimitiveComponent가 등록/해제.
// Native 또는 PhysX로 교체 가능.
// ============================================================
class IPhysicsScene
{
public:
	virtual ~IPhysicsScene() = default;

	// --- Lifecycle ---
	virtual void Initialize(UWorld* InWorld) = 0;
	virtual void Shutdown() = 0;

	// --- Body 관리 ---
	virtual void RegisterComponent(UPrimitiveComponent* Comp) = 0;
	virtual void UnregisterComponent(UPrimitiveComponent* Comp) = 0;
	// 컴포넌트의 SimulatePhysics/ObjectType/Response 등이 변경된 경우 호출.
	// PhysX는 actor 단위로 unregister + register (compound shape의 다른 컴포넌트도 함께 재등록),
	// Native는 BodyState만 갱신.
	virtual void RebuildBody(UPrimitiveComponent* Comp) = 0;

	// --- 시뮬레이션 ---
	virtual void Tick(float DeltaTime) = 0;

	// --- 힘/토크 ---
	virtual void AddForce(UPrimitiveComponent* Comp, const FVector& Force) = 0;
	virtual void AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation) = 0;
	virtual void AddTorque(UPrimitiveComponent* Comp, const FVector& Torque) = 0;

	// --- 속도 읽기/쓰기 ---
	virtual FVector GetLinearVelocity(UPrimitiveComponent* Comp) const = 0;
	virtual void SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel) = 0;
	virtual FVector GetAngularVelocity(UPrimitiveComponent* Comp) const = 0;
	virtual void SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel) = 0;

	// --- Mass / Center of Mass ---
	virtual void SetMass(UPrimitiveComponent* Comp, float Mass) = 0;
	virtual float GetMass(UPrimitiveComponent* Comp) const = 0;
	// CenterOfMass는 RootComponent의 local 좌표계 기준 offset.
	// 차량처럼 mass center를 차체 아래로 내리면 회전 안정성↑.
	virtual void SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset) = 0;
	virtual FVector GetCenterOfMass(UPrimitiveComponent* Comp) const = 0;

	// --- Raycast ---
	// TraceChannel: shape의 응답이 이 채널에 대해 Block일 때만 hit으로 인정 (UE 패턴).
	//   예: WorldStatic 채널로 trace → 응답이 WorldStatic Block인 shape만 hit.
	//   trigger flag가 set된 shape는 PhysX 측에서 자동 제외됨.
	// IgnoreActor: 자기 자신/소유 액터를 제외할 때 사용.
	virtual bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr) const = 0;
};
