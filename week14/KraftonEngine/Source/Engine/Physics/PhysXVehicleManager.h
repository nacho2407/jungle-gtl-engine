#pragma once

#include "Physics/PhysXInclude.h"

#include "Core/Types/CoreTypes.h"

class FPhysXVehicleInstance;

class FPhysXVehicleManager
{
public:
	void Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* DefualtMaterial);
	void Shutdown();

	void RegisterVehicle(FPhysXVehicleInstance* Vehicle);
	void UnregisterVehicle(FPhysXVehicleInstance* Vehicle);

	void Update(float DeltaTime);

private:
	void RebuildQueryBuffers();

private:
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;

	physx::PxBatchQuery* BatchQuery = nullptr;
	physx::PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	TArray<FPhysXVehicleInstance*> Vehicles;

	TArray<physx::PxVehicleWheels*> PxVehicles;

	TArray<physx::PxVehicleWheelQueryResult> WheelQueryResults;
	TArray<physx::PxRaycastQueryResult> RaycastResults;
	TArray<physx::PxRaycastHit> RaycastHitBuffer;
	TArray<physx::PxWheelQueryResult> WheelQueryResultStorage;

	physx::PxFilterData SuspensionQueryFilterData;
};
