#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Core/RayTypes.h"

class UWorld;
class UPrimitiveComponent;

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
// Native(기존 CollisionSystem) 또는 PhysX로 교체 가능.
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

	// --- 시뮬레이션 ---
	virtual void Tick(float DeltaTime) = 0;
};
