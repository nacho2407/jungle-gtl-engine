#include "Physics/PhysXVehicleManager.h"
#include "Physics/PhysicsFilterData.h"
#include "Physics/PhysXConversions.h"

#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Render/Scene/FScene.h"

#include <algorithm>

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
	WheelQueryResults.clear();
	RaycastResults.clear();
	Physics = nullptr;
	Scene = nullptr;
	DefaultMaterial = nullptr;
}

void FPhysXVehicleManager::RegisterVehicle(physx::PxVehicleWheels* Vehicle)
{
	if (!Vehicle) return;
	if (std::find(Vehicles.begin(), Vehicles.end(), Vehicle) != Vehicles.end()) return;
	Vehicles.push_back(Vehicle);
}

void FPhysXVehicleManager::UnregisterVehicle(physx::PxVehicleWheels* Vehicle)
{
	if (!Vehicle) return;
	Vehicles.erase(std::remove(Vehicles.begin(), Vehicles.end(), Vehicle), Vehicles.end());
}

void FPhysXVehicleManager::Update(float DeltaTime)
{
	if (!Scene || !FrictionPairs || Vehicles.empty()) return;

	RebuildQueryBuffers();

	if (!BatchQuery) return;

	physx::PxVehicleSuspensionRaycasts(BatchQuery, static_cast<physx::PxU32>(Vehicles.size()),
		Vehicles.data(), static_cast<physx::PxU32>(RaycastResults.size()), RaycastResults.data());

	const physx::PxVec3 Gravity = Scene->getGravity();

	physx::PxVehicleUpdates(DeltaTime, Gravity, *FrictionPairs, static_cast<physx::PxU32>(Vehicles.size()),
		Vehicles.data(), WheelQueryResults.data());
}

const physx::PxVehicleWheelQueryResult* FPhysXVehicleManager::GetWheelQueryResult(const physx::PxVehicleWheels* Vehicle) const
{
	if (!Vehicle)
	{
		return nullptr;
	}

	for (uint32 Index = 0; Index < static_cast<uint32>(Vehicles.size()); ++Index)
	{
		if (Vehicles[Index] == Vehicle && Index < WheelQueryResults.size())
		{
			const physx::PxVehicleWheelQueryResult& Result = WheelQueryResults[Index];
			return Result.wheelQueryResults ? &Result : nullptr;
		}
	}

	return nullptr;
}

void FPhysXVehicleManager::CollectDebugRender(FScene& RenderScene) const
{
	for (uint32 VehicleIndex = 0; VehicleIndex < static_cast<uint32>(Vehicles.size()); ++VehicleIndex)
	{
		physx::PxVehicleWheels* Vehicle = Vehicles[VehicleIndex];
		if (!Vehicle || VehicleIndex >= WheelQueryResults.size()) continue;

		const physx::PxVehicleWheelQueryResult& VehicleQuery = WheelQueryResults[VehicleIndex];
		const physx::PxU32 WheelCount = VehicleQuery.nbWheelQueryResults;

		for (physx::PxU32 WheelIndex = 0; WheelIndex < WheelCount; ++WheelIndex)
		{
			const physx::PxWheelQueryResult& WheelQuery = VehicleQuery.wheelQueryResults[WheelIndex];

			const physx::PxRigidDynamic* Actor = Vehicle->getRigidDynamicActor();
			if (!Actor) continue;

			const physx::PxTransform ActorPose = Actor->getGlobalPose();
			const physx::PxTransform WheelWorldPose = ActorPose.transform(WheelQuery.localPose);

			const FVector Center = FromPxVec3(WheelWorldPose.p);

			physx::PxVehicleWheelData WheelData = Vehicle->mWheelsSimData.getWheelData(WheelIndex);
			const float Radius = WheelData.mRadius;

			const FVector Right = FromPxVec3(WheelWorldPose.q.rotate(physx::PxVec3(0, 1, 0)));
			const FVector Up = FromPxVec3(WheelWorldPose.q.rotate(physx::PxVec3(0, 0, 1)));

			RenderScene.AddDebugLine(Center - Right * Radius, Center + Right * Radius, FColor(255, 200, 0));
			RenderScene.AddDebugLine(Center - Up * Radius, Center + Up * Radius, FColor(255, 200, 0));

			const FVector SuspDir = FromPxVec3(ActorPose.q.rotate(physx::PxVec3(0, 0, -1)));
			const FVector RayStart = Center;
			const FVector RayEnd = WheelQuery.isInAir ? Center + SuspDir * (Radius + 0.5f)
				: FromPxVec3(WheelQuery.tireContactPoint);

			RenderScene.AddDebugLine(RayStart, RayEnd, WheelQuery.isInAir ? FColor(255, 80, 80) : FColor(80, 255, 80));

			if (!WheelQuery.isInAir)
			{
				const FVector Contact = FromPxVec3(WheelQuery.tireContactPoint);
				const FVector Normal = FromPxVec3(WheelQuery.tireContactNormal);
				RenderScene.AddDebugLine(Contact, Contact + Normal * 0.5f, FColor(80, 160, 255));
			}
		}
	}
}

void FPhysXVehicleManager::RebuildQueryBuffers()
{
	physx::PxU32 TotalWheelCount = 0;

	for (physx::PxVehicleWheels* Vehicle : Vehicles)
	{
		TotalWheelCount += Vehicle->mWheelsSimData.getNbWheels();
	}

	if (TotalWheelCount == 0) return;

	RaycastResults.resize(TotalWheelCount);
	RaycastHitBuffer.resize(TotalWheelCount);
	WheelQueryResults.resize(Vehicles.size());
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
	for (uint32 Index = 0; Index < static_cast<uint32>(Vehicles.size()); ++Index)
	{
		const physx::PxU32 WheelCount = Vehicles[Index]->mWheelsSimData.getNbWheels();

		WheelQueryResults[Index].nbWheelQueryResults = WheelCount;
		WheelQueryResults[Index].wheelQueryResults = WheelQueryResultStorage.data() + WheelOffset;

		WheelOffset += WheelCount;
	}
}
