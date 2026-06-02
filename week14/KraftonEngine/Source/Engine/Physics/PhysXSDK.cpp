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

	physx::PxCookingParams CookingParams(Physics->getTolerancesScale());
	Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *Foundation, CookingParams);

	if (!Cooking)
	{
		Physics->release();
		Physics = nullptr;
		Foundation->release();
		Foundation = nullptr;
		return;
	}

	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);

	physx::PxInitVehicleSDK(*Physics);
	physx::PxVehicleSetBasisVectors(physx::PxVec3(0, 0, 1), physx::PxVec3(1, 0, 0));
	physx::PxVehicleSetUpdateMode(physx::PxVehicleUpdateMode::eVELOCITY_CHANGE);
}

void FPhysXSDK::Shutdown()
{
	if (DefaultMaterial)
	{
		DefaultMaterial->release();
		DefaultMaterial = nullptr;
	}
	if (Cooking)
	{
		Cooking->release();
		Cooking = nullptr;
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
