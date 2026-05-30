#include "Physics/PhysXSDK.h"

void FPhysXSDK::Initialize()
{
	if (Physics) return;

	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);
	if (!Foundation) return;

	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, physx::PxTolerancesScale());
	if (!Physics)
	{
		Foundation->release();
		Foundation = nullptr;
		return;
	}
	
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
