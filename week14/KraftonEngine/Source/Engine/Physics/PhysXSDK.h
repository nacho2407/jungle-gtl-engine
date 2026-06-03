#pragma once

#include "Core/Singleton.h"
#include "Physics/PhysXInclude.h"

class FPhysXSDK : public TSingleton<FPhysXSDK>
{
	friend class TSingleton<FPhysXSDK>;
public:
	void Initialize();
	void Shutdown();

	physx::PxPhysics* GetPhysics() const { return Physics; }
	physx::PxFoundation* GetFoundation() const { return Foundation; }
	physx::PxMaterial* GetDefaultMaterial() const { return DefaultMaterial; }
	
	physx::PxCooking* GetCooking() const { return Cooking; }

	physx::PxAllocatorCallback* GetAllocatorCallback() { return &Allocator; }
	physx::PxErrorCallback* GetErrorCallback() { return &ErrorCallback; }

	physx::PxPvd* GetPvd() const { return Pvd; }

private:
	physx::PxDefaultAllocator Allocator;
	physx::PxDefaultErrorCallback ErrorCallback;

	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;

	physx::PxCooking* Cooking = nullptr;

	physx::PxPvd* Pvd = nullptr;
	physx::PxPvdTransport* PvdTransport = nullptr;
};
