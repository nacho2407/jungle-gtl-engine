#include "Physics/PhysXSDK.h"

#include "Core/Logging/Log.h"
#include "Core/ProjectSettings.h"

void FPhysXSDK::Initialize()
{
	if (Physics) return;

	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);
	if (!Foundation) return;

	const bool bEnablePvd = FProjectSettings::Get().Physics.bEnablePvd;
	if (bEnablePvd)
	{
		Pvd = physx::PxCreatePvd(*Foundation);
		if (Pvd)
		{
			PvdTransport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
			if (PvdTransport)
			{
				const bool bConnected = Pvd->connect(*PvdTransport, physx::PxPvdInstrumentationFlag::eALL);
				UE_LOG("PVD connect: %s", bConnected ? "OK" : "FAILED");
			}
		}
	}

	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, physx::PxTolerancesScale(), true, Pvd);
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

	if (bEnablePvd)
	{
		PxInitExtensions(*Physics, Pvd);
	}

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

	const bool bEnablePvd = FProjectSettings::Get().Physics.bEnablePvd;
	if (bEnablePvd)
	{
		PxCloseExtensions();
	}

	if (Physics)
	{
		Physics->release();
		Physics = nullptr;
	}
	if (Pvd)
	{
		Pvd->disconnect();
		Pvd->release();
		Pvd = nullptr;
	}
	if (PvdTransport)
	{
		PvdTransport->release();
		PvdTransport = nullptr;
	}
	if (Foundation)
	{
		Foundation->release();
		Foundation = nullptr;
	}
}
