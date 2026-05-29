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

private:
	physx::PxDefaultAllocator Allocator;
	physx::PxDefaultErrorCallback ErrorCallback;

	physx::PxFoundation* Foundation = nullptr;
	physx::PxPhysics* Physics = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;
};
