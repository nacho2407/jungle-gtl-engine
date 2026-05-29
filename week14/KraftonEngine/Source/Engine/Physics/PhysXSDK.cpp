#include "Physics/PhysXSDK.h"

void FPhysXSDK::Initialize()
{
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, physx::PxTolerancesScale());
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);
}

void FPhysXSDK::Shutdown()
{
	if (DefaultMaterial)
	{
		DefaultMaterial->release();
		DefaultMaterial = nullptr;
	}
	if (Physics)
	{
		Physics->release();
		Physics = nullptr;
	}
	if (Foundation)
	{
		Foundation->release();
		Foundation = nullptr;
	}
}
