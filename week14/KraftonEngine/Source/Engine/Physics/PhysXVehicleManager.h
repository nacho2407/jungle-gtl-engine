#pragma once

#include "Physics/PhysXInclude.h"

#include "Core/Types/CoreTypes.h"

class FScene;
struct FClothCollisionGatherParams;
struct FClothCollisionData;

class FPhysXVehicleManager
{
public:
	void Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* DefaultMaterial);
	void Shutdown();

	void RegisterVehicle(physx::PxVehicleWheels* Vehicle);
	void UnregisterVehicle(physx::PxVehicleWheels* Vehicle);

	void Update(float DeltaTime);

	void GatherClothCollision(const FClothCollisionGatherParams& Params, FClothCollisionData& OutCollisionData) const;
	const physx::PxVehicleWheelQueryResult* GetWheelQueryResult(const physx::PxVehicleWheels* Vehicle) const;

	void CollectDebugRender(FScene& RenderScene) const;

private:
	void RebuildQueryBuffers();

private:
	physx::PxPhysics* Physics = nullptr;
	physx::PxScene* Scene = nullptr;
	physx::PxMaterial* DefaultMaterial = nullptr;

	physx::PxBatchQuery* BatchQuery = nullptr;
	physx::PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	TArray<physx::PxVehicleWheels*> Vehicles;

	TArray<physx::PxVehicleWheelQueryResult> WheelQueryResults;
	TArray<physx::PxRaycastQueryResult> RaycastResults;
	TArray<physx::PxRaycastHit> RaycastHitBuffer;
	TArray<physx::PxWheelQueryResult> WheelQueryResultStorage;

	physx::PxFilterData SuspensionQueryFilterData;
};
