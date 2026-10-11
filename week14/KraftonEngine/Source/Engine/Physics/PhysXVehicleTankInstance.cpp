#include "Physics/PhysXVehicleTankInstance.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/MathUtils.h"
#include "Physics/PhysXConversions.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	int32 ClampRoadWheelCount(const int32 Count)
	{
		return std::max(2, std::min(10, Count));
	}

	float ClampUnit(const float Value)
	{
		return FMath::Clamp(Value, -1.0f, 1.0f);
	}

	float ClampPositiveUnit(const float Value)
	{
		return FMath::Clamp(Value, 0.0f, 1.0f);
	}
}

bool FPhysXVehicleTankInstance::Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose, const FTankVehiclePhysicsSetup& Setup)
{
	if (!Physics || !Scene || !Material)
	{
		return false;
	}

	Shutdown();

	const int32 WheelsPerSide = ClampRoadWheelCount(Setup.RoadWheelCountPerSide);
	WheelCount = static_cast<uint32>(WheelsPerSide * 2);
	WheelRadius = std::max(0.01f, Setup.WheelRadius);
	LeftTrackSpeed = 0.0f;
	RightTrackSpeed = 0.0f;
	WheelRestLocalZ.assign(WheelCount, 0.0f);
	WheelRotationAngles.assign(WheelCount, 0.0f);
	WheelRotationSpeeds.assign(WheelCount, 0.0f);
	WheelSuspensionOffsets.assign(WheelCount, 0.0f);

	const float ChassisMass = std::max(1.0f, Setup.ChassisMass);
	const physx::PxVec3 ChassisDims(std::max(0.01f, Setup.ChassisHalfExtent.X),
		std::max(0.01f, Setup.ChassisHalfExtent.Y),
		std::max(0.01f, Setup.ChassisHalfExtent.Z));

	VehicleActor = Physics->createRigidDynamic(StartPose);
	if (!VehicleActor)
	{
		return false;
	}

	VehicleActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);

	for (uint32 Index = 0; Index < WheelCount; ++Index)
	{
		physx::PxShape* WheelShape = Physics->createShape(physx::PxSphereGeometry(WheelRadius), *Material, true);
		if (!WheelShape)
		{
			Shutdown();
			return false;
		}

		WheelShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		WheelShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		VehicleActor->attachShape(*WheelShape);
		WheelShape->release();
	}

	physx::PxShape* ChassisShape = Physics->createShape(physx::PxBoxGeometry(ChassisDims), *Material);
	if (!ChassisShape)
	{
		Shutdown();
		return false;
	}

	VehicleActor->attachShape(*ChassisShape);
	ChassisShape->release();

	physx::PxRigidBodyExt::setMassAndUpdateInertia(*VehicleActor, ChassisMass);
	VehicleActor->setLinearDamping(std::max(0.0f, Setup.LinearDamping));
	VehicleActor->setAngularDamping(std::max(0.0f, Setup.AngularDamping));

	physx::PxTransform CenterOfMassOffset(physx::PxIdentity);
	CenterOfMassOffset.p = ToPxVec3(Setup.CenterOfMassOffset);
	VehicleActor->setCMassLocalPose(CenterOfMassOffset);

	physx::PxVehicleWheelsSimData* WheelsSimData = physx::PxVehicleWheelsSimData::allocate(WheelCount);
	if (!WheelsSimData)
	{
		Shutdown();
		return false;
	}

	std::vector<physx::PxVec3> WheelCentreOffsets(WheelCount);
	const float TrackHalfWidth = std::max(0.01f, Setup.TrackHalfWidth);
	const float WheelCenterZ = -ChassisDims.z;

	for (int32 PairIndex = 0; PairIndex < WheelsPerSide; ++PairIndex)
	{
		const float Alpha = static_cast<float>(PairIndex) / static_cast<float>(WheelsPerSide - 1);
		const float WheelX = FMath::Lerp(Setup.FrontWheelOffsetX, Setup.RearWheelOffsetX, Alpha);
		const uint32 LeftIndex = static_cast<uint32>(PairIndex * 2);
		const uint32 RightIndex = LeftIndex + 1;

		WheelCentreOffsets[LeftIndex] = physx::PxVec3(WheelX, -TrackHalfWidth, WheelCenterZ);
		WheelCentreOffsets[RightIndex] = physx::PxVec3(WheelX, TrackHalfWidth, WheelCenterZ);
		WheelRestLocalZ[LeftIndex] = WheelCenterZ;
		WheelRestLocalZ[RightIndex] = WheelCenterZ;
	}

	std::vector<physx::PxF32> SprungMasses(WheelCount);
	physx::PxVehicleComputeSprungMasses(WheelCount, WheelCentreOffsets.data(),
		ToPxVec3(Setup.CenterOfMassOffset), ChassisMass, 2, SprungMasses.data());

	for (uint32 Index = 0; Index < WheelCount; ++Index)
	{
		physx::PxVehicleWheelData Wheel;
		Wheel.mMass = std::max(0.01f, Setup.WheelMass);
		Wheel.mRadius = WheelRadius;
		Wheel.mWidth = std::max(0.01f, Setup.WheelWidth);
		Wheel.mMOI = 0.5f * Wheel.mMass * Wheel.mRadius * Wheel.mRadius;
		Wheel.mDampingRate = std::max(0.0f, Setup.WheelDampingRate);
		Wheel.mMaxSteer = 0.0f;
		Wheel.mMaxBrakeTorque = std::max(0.0f, Setup.MaxBrakeTorque);
		Wheel.mMaxHandBrakeTorque = Wheel.mMaxBrakeTorque;
		WheelsSimData->setWheelData(Index, Wheel);

		physx::PxVehicleSuspensionData Suspension;
		Suspension.mMaxCompression = std::max(0.0f, Setup.SuspensionMaxCompression);
		Suspension.mMaxDroop = std::max(0.0f, Setup.SuspensionMaxDroop);
		Suspension.mSpringStrength = std::max(0.0f, Setup.SuspensionSpringStrength);
		Suspension.mSpringDamperRate = std::max(0.0f, Setup.SuspensionSpringDamperRate);
		Suspension.mSprungMass = SprungMasses[Index];
		WheelsSimData->setSuspensionData(Index, Suspension);

		physx::PxVehicleTireData Tire;
		Tire.mType = 0;
		WheelsSimData->setTireData(Index, Tire);

		const physx::PxVec3& WheelOffset = WheelCentreOffsets[Index];
		const physx::PxVec3 ForceAppPointOffset(WheelOffset.x, WheelOffset.y, Setup.CenterOfMassOffset.Z);

		WheelsSimData->setWheelCentreOffset(Index, WheelOffset);
		WheelsSimData->setSuspForceAppPointOffset(Index, ForceAppPointOffset);
		WheelsSimData->setTireForceAppPointOffset(Index, ForceAppPointOffset);
		WheelsSimData->setSuspTravelDirection(Index, physx::PxVec3(0.0f, 0.0f, -1.0f));
		WheelsSimData->setWheelShapeMapping(Index, Index);

		physx::PxFilterData SuspensionQueryFilterData;
		SuspensionQueryFilterData.word0 = ObjectTypeBit(ECollisionChannel::WorldStatic);
		WheelsSimData->setSceneQueryFilterData(Index, SuspensionQueryFilterData);
	}

	physx::PxVehicleDriveSimData DriveData;

	physx::PxVehicleEngineData Engine;
	Engine.mPeakTorque = std::max(0.0f, Setup.EnginePeakTorque);
	Engine.mMaxOmega = std::max(0.01f, Setup.EngineMaxOmega);
	DriveData.setEngineData(Engine);

	physx::PxVehicleGearsData Gears;
	DriveData.setGearsData(Gears);

	physx::PxVehicleClutchData Clutch;
	Clutch.mStrength = std::max(0.0f, Setup.ClutchStrength);
	DriveData.setClutchData(Clutch);

	Vehicle = physx::PxVehicleDriveTank::allocate(WheelCount);
	if (!Vehicle)
	{
		WheelsSimData->free();
		Shutdown();
		return false;
	}

	Vehicle->setup(Physics, VehicleActor, *WheelsSimData, DriveData, WheelCount);
	Vehicle->setDriveModel(physx::PxVehicleDriveTankControlModel::eSPECIAL);
	Vehicle->setToRestState();
	Vehicle->mDriveDynData.setUseAutoGears(true);
	Vehicle->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eFIRST);

	WheelsSimData->free();

	Scene->addActor(*VehicleActor);
	return true;
}

void FPhysXVehicleTankInstance::Shutdown()
{
	if (Vehicle)
	{
		Vehicle->free();
		Vehicle = nullptr;
	}

	if (VehicleActor)
	{
		VehicleActor->release();
		VehicleActor = nullptr;
	}

	WheelCount = 0;
	WheelRadius = 0.0f;
	LeftTrackSpeed = 0.0f;
	RightTrackSpeed = 0.0f;
	WheelRestLocalZ.clear();
	WheelRotationAngles.clear();
	WheelRotationSpeeds.clear();
	WheelSuspensionOffsets.clear();
}

void FPhysXVehicleTankInstance::SetDriveInput(const float Throttle, const float Brake, const float Steer, const bool bReverse)
{
	const float ClampedThrottle = ClampUnit(Throttle);
	const float Drive = bReverse ? -std::abs(ClampedThrottle) : ClampedThrottle;
	const float Turn = ClampUnit(Steer);

	const float LeftThrust = ClampUnit(Drive + Turn);
	const float RightThrust = ClampUnit(Drive - Turn);
	const float ClampedBrake = ClampPositiveUnit(Brake);

	SetTrackInput(LeftThrust, RightThrust, ClampedBrake, ClampedBrake);
}

void FPhysXVehicleTankInstance::SetTrackInput(const float LeftThrust, const float RightThrust,
	const float LeftBrake, const float RightBrake)
{
	if (!Vehicle)
	{
		return;
	}

	const float ClampedLeftThrust = ClampUnit(LeftThrust);
	const float ClampedRightThrust = ClampUnit(RightThrust);
	const float ClampedLeftBrake = ClampPositiveUnit(LeftBrake);
	const float ClampedRightBrake = ClampPositiveUnit(RightBrake);
	const float Accel = std::max(std::abs(ClampedLeftThrust), std::abs(ClampedRightThrust));

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDriveTankControl::eANALOG_INPUT_ACCEL, Accel);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDriveTankControl::eANALOG_INPUT_THRUST_LEFT, ClampedLeftThrust);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDriveTankControl::eANALOG_INPUT_THRUST_RIGHT, ClampedRightThrust);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDriveTankControl::eANALOG_INPUT_BRAKE_LEFT, ClampedLeftBrake);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDriveTankControl::eANALOG_INPUT_BRAKE_RIGHT, ClampedRightBrake);

	UpdateVisualState();
}

void FPhysXVehicleTankInstance::FireRecoil(const float Impulse, const FVector& LocalFirePoint, const FVector& LocalDirection)
{
	if (!VehicleActor || Impulse <= 0.0f)
	{
		return;
	}

	physx::PxVec3 LocalDir = ToPxVec3(LocalDirection);
	if (LocalDir.magnitudeSquared() <= 1.0e-6f)
	{
		return;
	}

	LocalDir.normalize();

	const physx::PxTransform ActorPose = VehicleActor->getGlobalPose();
	const physx::PxVec3 WorldDirection = ActorPose.q.rotate(LocalDir);
	const physx::PxVec3 WorldFirePoint = ActorPose.transform(ToPxVec3(LocalFirePoint));
	const physx::PxVec3 RecoilImpulse = -WorldDirection * Impulse;

	physx::PxRigidBodyExt::addForceAtPos(*VehicleActor, RecoilImpulse, WorldFirePoint, physx::PxForceMode::eIMPULSE);
}

void FPhysXVehicleTankInstance::UpdateVisualState(const physx::PxVehicleWheelQueryResult* WheelQueryResult)
{
	if (!Vehicle || WheelCount == 0)
	{
		LeftTrackSpeed = 0.0f;
		RightTrackSpeed = 0.0f;
		WheelRotationAngles.clear();
		WheelRotationSpeeds.clear();
		WheelSuspensionOffsets.clear();
		return;
	}

	if (WheelRotationAngles.size() != WheelCount)
	{
		WheelRotationAngles.assign(WheelCount, 0.0f);
	}
	if (WheelRotationSpeeds.size() != WheelCount)
	{
		WheelRotationSpeeds.assign(WheelCount, 0.0f);
	}
	if (WheelSuspensionOffsets.size() != WheelCount)
	{
		WheelSuspensionOffsets.assign(WheelCount, 0.0f);
	}

	float LeftSum = 0.0f;
	float RightSum = 0.0f;
	uint32 LeftCount = 0;
	uint32 RightCount = 0;

	for (uint32 Index = 0; Index < WheelCount; ++Index)
	{
		const float WheelRotationAngle = Vehicle->mWheelsDynData.getWheelRotationAngle(Index);
		const float WheelRotationSpeed = Vehicle->mWheelsDynData.getWheelRotationSpeed(Index);
		const float WheelSpeed = WheelRotationSpeed * WheelRadius;

		WheelRotationAngles[Index] = WheelRotationAngle;
		WheelRotationSpeeds[Index] = WheelRotationSpeed;
		WheelSuspensionOffsets[Index] = 0.0f;

		if (WheelQueryResult && WheelQueryResult->wheelQueryResults && Index < WheelQueryResult->nbWheelQueryResults)
		{
			const physx::PxWheelQueryResult& Query = WheelQueryResult->wheelQueryResults[Index];
			const float RestZ = Index < WheelRestLocalZ.size() ? WheelRestLocalZ[Index] : Query.localPose.p.z;
			WheelSuspensionOffsets[Index] = Query.localPose.p.z - RestZ;
		}

		if ((Index % 2) == 0)
		{
			LeftSum += WheelSpeed;
			++LeftCount;
		}
		else
		{
			RightSum += WheelSpeed;
			++RightCount;
		}
	}

	LeftTrackSpeed = LeftCount > 0 ? LeftSum / static_cast<float>(LeftCount) : 0.0f;
	RightTrackSpeed = RightCount > 0 ? RightSum / static_cast<float>(RightCount) : 0.0f;
}

float FPhysXVehicleTankInstance::GetWheelRotationAngle(const uint32 WheelIndex) const
{
	if (!Vehicle || WheelIndex >= WheelRotationAngles.size())
	{
		return 0.0f;
	}

	return WheelRotationAngles[WheelIndex];
}

float FPhysXVehicleTankInstance::GetWheelRotationSpeed(const uint32 WheelIndex) const
{
	if (!Vehicle || WheelIndex >= WheelRotationSpeeds.size())
	{
		return 0.0f;
	}

	return WheelRotationSpeeds[WheelIndex];
}

float FPhysXVehicleTankInstance::GetWheelSuspensionOffset(const uint32 WheelIndex) const
{
	if (!Vehicle || WheelIndex >= WheelSuspensionOffsets.size())
	{
		return 0.0f;
	}

	return WheelSuspensionOffsets[WheelIndex];
}
