#pragma once

#include "Physics/PhysicsTypes.h"
#include "Physics/PhysXInclude.h"

#include "Core/Types/CoreTypes.h"

class UPrimitiveComponent;
struct FBodyInstance;

class FPhysicsScene
{
public:
	void Initialize();
	void Shutdown();
	void Simulate(float DeltaTime);

	FBodyInstance* CreateBody(UPrimitiveComponent* OwnerComp);
	void DestroyBody(FBodyInstance* Instance);

	physx::PxScene* GetPxScene() const { return Scene; }

private:
	physx::PxDefaultAllocator Allocator;
	physx::PxDefaultErrorCallback ErrorCallback;

	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;

	TArray<FBodyInstance*> Bodies;
};
