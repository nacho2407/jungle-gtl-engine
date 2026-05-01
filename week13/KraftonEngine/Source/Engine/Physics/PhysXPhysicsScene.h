#pragma once

#include "Physics/IPhysicsScene.h"
#include <vector>

// Forward declarations — PhysX types
namespace physx
{
	class PxFoundation;
	class PxPhysics;
	class PxScene;
	class PxDefaultCpuDispatcher;
	class PxMaterial;
	class PxRigidActor;
}

class FPhysXSimulationCallback;

// ============================================================
// FPhysXPhysicsScene — PhysX 4.1 기반 물리 시스템
//
// IPhysicsScene 인터페이스를 통해 Native와 교체 가능.
// ============================================================
class FPhysXPhysicsScene : public IPhysicsScene
{
public:
	void Initialize(UWorld* InWorld) override;
	void Shutdown() override;

	void RegisterComponent(UPrimitiveComponent* Comp) override;
	void UnregisterComponent(UPrimitiveComponent* Comp) override;

	void Tick(float DeltaTime) override;

private:
	UWorld* World = nullptr;

	// PhysX core objects
	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
	FPhysXSimulationCallback* EventCallback = nullptr;

	// Component ↔ PhysX body 매핑
	struct FBodyMapping
	{
		UPrimitiveComponent* Component = nullptr;
		physx::PxRigidActor* Actor = nullptr;
	};
	std::vector<FBodyMapping> BodyMappings;

	// 내부 헬퍼
	physx::PxRigidActor* CreateBodyForComponent(UPrimitiveComponent* Comp);
	void RemoveBody(physx::PxRigidActor* Body);
	FBodyMapping* FindMapping(UPrimitiveComponent* Comp);
};
