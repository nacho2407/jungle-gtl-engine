#include "Physics/PhysXVehicleManager.h"
#include "Physics/PhysXVehicleInstance.h"
#include "Physics/PhysicsFilterData.h"

#include "Core/Types/CollisionTypes.h"

namespace
{
	physx::PxQueryHitType::Enum VehicleSuspensionHitFilter(physx::PxFilterData QueryFilterData,
		physx::PxFilterData ShapeFilterData, const void* ConstantBlock, physx::PxU32 ConstantBlockSize,
		physx::PxHitFlags& QueryFlags)
	{
		const bool bQueryEnabled = (ShapeFilterData.word3 & EPhysicsFilterFlags::QueryOnly) ||
			(ShapeFilterData.word3 & EPhysicsFilterFlags::QueryAndPhysics);

		if (!bQueryEnabled) return physx::PxQueryHitType::eNONE;

		const physx::PxU32 TraceChannelBit = QueryFilterData.word0;
		const bool bShapeBlocksTrace = (ShapeFilterData.word1 & TraceChannelBit) != 0;

		if (!bShapeBlocksTrace) return physx::PxQueryHitType::eNONE;

		return physx::PxQueryHitType::eBLOCK;
	}
}

void FPhysXVehicleManager::Initialize(physx::PxPhysics* InPhysics, physx::PxScene* InScene, physx::PxMaterial* InDefaultMaterial)
{
	Physics = InPhysics;
	Scene = InScene;
	DefaultMaterial = InDefaultMaterial;

	physx::PxVehicleDrivableSurfaceType SurfaceTypes[1];
	SurfaceTypes[0].mType = 0;

	const physx::PxMaterial* SurfaceMaterials[1] = { DefaultMaterial };

	FrictionPairs = physx::PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);
	FrictionPairs->setup(1, 1, SurfaceMaterials, SurfaceTypes);
	FrictionPairs->setTypePairFriction(0, 0, 1.0f);

	SuspensionQueryFilterData = physx::PxFilterData();
	SuspensionQueryFilterData.word0 = ObjectTypeBit(ECollisionChannel::WorldStatic);
}

void FPhysXVehicleManager::Shutdown()
{
	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}
	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}
	Vehicles.clear();
	PxVehicles.clear();
	WheelQueryResults.clear();
	RaycastResults.clear();
	Physics = nullptr;
	Scene = nullptr;
	DefaultMaterial = nullptr;
}

void FPhysXVehicleManager::RegisterVehicle(FPhysXVehicleInstance* Vehicle)
{
	if (!Vehicle) return;
	Vehicles.push_back(Vehicle);
}

void FPhysXVehicleManager::UnregisterVehicle(FPhysXVehicleInstance* Vehicle)
{
	if (!Vehicle) return;
	Vehicles.erase(std::remove(Vehicles.begin(), Vehicles.end(), Vehicle), Vehicles.end());
}

void FPhysXVehicleManager::Update(float DeltaTime)
{
	if (!Scene || !FrictionPairs || Vehicles.empty()) return;

	PxVehicles.clear();

	for (FPhysXVehicleInstance* VehicleInstance : Vehicles)
	{
		if (!VehicleInstance) continue;

		physx::PxVehicleWheels* Vehicle = VehicleInstance->GetPxVehicle();
		if (!Vehicle) continue;

		PxVehicles.push_back(Vehicle);
	}

	if (PxVehicles.empty()) return;

	RebuildQueryBuffers();

	if (!BatchQuery) return;

	physx::PxVehicleSuspensionRaycasts(BatchQuery, static_cast<physx::PxU32>(PxVehicles.size()),
		PxVehicles.data(), static_cast<physx::PxU32>(RaycastResults.size()), RaycastResults.data());

	const physx::PxVec3 Gravity = Scene->getGravity();

	physx::PxVehicleUpdates(DeltaTime, Gravity, *FrictionPairs, static_cast<physx::PxU32>(PxVehicles.size()),
		PxVehicles.data(), WheelQueryResults.data());
}

void FPhysXVehicleManager::RebuildQueryBuffers()
{
	physx::PxU32 TotalWheelCount = 0;

	for (physx::PxVehicleWheels* Vehicle : PxVehicles)
	{
		TotalWheelCount += Vehicle->mWheelsSimData.getNbWheels();
	}

	if (TotalWheelCount == 0) return;

	RaycastResults.resize(TotalWheelCount);
	RaycastHitBuffer.resize(TotalWheelCount);
	WheelQueryResults.resize(PxVehicles.size());
	WheelQueryResultStorage.resize(TotalWheelCount);

	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}

	physx::PxBatchQueryDesc QueryDesc(TotalWheelCount, 0, 0);
	QueryDesc.queryMemory.userRaycastResultBuffer = RaycastResults.data();
	QueryDesc.queryMemory.userRaycastTouchBuffer = RaycastHitBuffer.data();
	QueryDesc.queryMemory.raycastTouchBufferSize = TotalWheelCount;
	QueryDesc.preFilterShader = VehicleSuspensionHitFilter;

	BatchQuery = Scene ? Scene->createBatchQuery(QueryDesc) : nullptr;

	physx::PxU32 WheelOffset = 0;
	for (uint32 Index = 0; Index < static_cast<uint32>(PxVehicles.size()); ++Index)
	{
		const physx::PxU32 WheelCount = PxVehicles[Index]->mWheelsSimData.getNbWheels();

		WheelQueryResults[Index].nbWheelQueryResults = WheelCount;
		WheelQueryResults[Index].wheelQueryResults = WheelQueryResultStorage.data() + WheelOffset;

		WheelOffset += WheelCount;
	}
}
