#include "Physics/PhysXVehicleInstance.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/MathUtils.h"

bool FPhysXVehicleInstance::Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	constexpr physx::PxU32 NumWheels = 4;

	const float ChassisMass = 1200.0f;
	const physx::PxVec3 ChassisDims(2.4f, 1.2f, 0.5f);

	const float WheelMass = 20.0f;
	const float WheelRadius = 0.35f;
	const float WheelWidth = 0.25f;

	const physx::PxVec3 WheelCentreOffset[NumWheels] =
	{
		physx::PxVec3(0.9f, -0.65f, -0.35f), // front left
		physx::PxVec3(0.9f,  0.65f, -0.35f), // front right
		physx::PxVec3(-0.9f, -0.65f, -0.35f), // rear left
		physx::PxVec3(-0.9f,  0.65f, -0.35f)  // rear right
	};

	VehicleActor = Physics->createRigidDynamic(StartPose);
	if (!VehicleActor) return false;

	VehicleActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);

	for (physx::PxU32 Index = 0; Index < NumWheels; ++Index)
	{
		physx::PxShape* WheelShape = Physics->createShape(physx::PxSphereGeometry(WheelRadius), *Material);

		WheelShape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
		WheelShape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
		VehicleActor->attachShape(*WheelShape);
		WheelShape->release();
	}

	physx::PxShape* ChassisShape = Physics->createShape(physx::PxBoxGeometry(ChassisDims), *Material);

	VehicleActor->attachShape(*ChassisShape);
	ChassisShape->release();

	physx::PxRigidBodyExt::setMassAndUpdateInertia(*VehicleActor, ChassisMass);
	VehicleActor->setLinearDamping(0.1f);
	VehicleActor->setAngularDamping(0.5f);

	physx::PxTransform CenterOfMassOffset;
	CenterOfMassOffset.p = physx::PxVec3(0, 0, -0.3f);
	VehicleActor->setCMassLocalPose(CenterOfMassOffset);

	physx::PxVehicleWheelsSimData* WheelsSimData = physx::PxVehicleWheelsSimData::allocate(NumWheels);

	for (physx::PxU32 Index = 0; Index < NumWheels; ++Index)
	{
		physx::PxVehicleWheelData Wheel;
		Wheel.mMass = WheelMass;
		Wheel.mRadius = WheelRadius;
		Wheel.mWidth = WheelWidth;
		Wheel.mMOI = 0.5f * WheelMass * WheelRadius * WheelRadius;
		Wheel.mDampingRate = 0.25f;

		if (Index == 0 || Index == 1)
		{
			Wheel.mMaxSteer = physx::PxPi * 0.25f;
		}
		else
		{
			Wheel.mMaxSteer = 0.0f;
		}

		WheelsSimData->setWheelData(Index, Wheel);

		physx::PxVehicleSuspensionData Suspension;
		Suspension.mMaxCompression = 0.3f;
		Suspension.mMaxDroop = 0.1f;
		Suspension.mSpringStrength = 35000.0f;
		Suspension.mSpringDamperRate = 4500.0f;
		Suspension.mSprungMass = ChassisMass / 4.0f;
		WheelsSimData->setSuspensionData(Index, Suspension);

		physx::PxVehicleTireData Tire;
		Tire.mType = 0;
		WheelsSimData->setTireData(Index, Tire);

		WheelsSimData->setWheelCentreOffset(Index, WheelCentreOffset[Index]);
		WheelsSimData->setSuspTravelDirection(Index, physx::PxVec3(0, 0, -1));
		WheelsSimData->setWheelShapeMapping(Index, Index);

		physx::PxFilterData SuspensionQueryFilterData;
		SuspensionQueryFilterData.word0 = ObjectTypeBit(ECollisionChannel::WorldStatic);
		WheelsSimData->setSceneQueryFilterData(Index, SuspensionQueryFilterData);
	}

	physx::PxVehicleDriveSimData4W DriveData;

	physx::PxVehicleEngineData Engine;
	Engine.mPeakTorque = 500.0f;
	Engine.mMaxOmega = 600.0f;
	DriveData.setEngineData(Engine);

	physx::PxVehicleGearsData Gears;
	DriveData.setGearsData(Gears);

	physx::PxVehicleClutchData Clutch;
	Clutch.mStrength = 10.0f;
	DriveData.setClutchData(Clutch);

	physx::PxVehicleDifferential4WData Diff;
	Diff.mType = physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
	DriveData.setDiffData(Diff);
	
	physx::PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = 1.0f;
	Ackermann.mAxleSeparation = 1.8f;
	Ackermann.mFrontWidth = 1.3f;
	Ackermann.mRearWidth = 1.3f;
	DriveData.setAckermannGeometryData(Ackermann);

	Vehicle = physx::PxVehicleDrive4W::allocate(NumWheels);
	Vehicle->setup(Physics, VehicleActor, *WheelsSimData, DriveData, NumWheels - 4);
	Vehicle->setToRestState();
	Vehicle->mDriveDynData.setUseAutoGears(true);

	WheelsSimData->free();

	Scene->addActor(*VehicleActor);
	return true;
}

void FPhysXVehicleInstance::Shutdown()
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
}

void FPhysXVehicleInstance::SetDriveInput(float Throttle, float Brake, float Steer)
{
	if (!Vehicle) return;

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL,
		FMath::Clamp(Throttle, 0.0f, 1.0f));

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE,
		FMath::Clamp(Brake, 0.0f, 1.0f));

	const float ClampedSteer = FMath::Clamp(Steer, -1.0f, 1.0f);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT,
		ClampedSteer < 0.0f ? -ClampedSteer : 0.0f);
	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT,
		ClampedSteer > 0.0f ? ClampedSteer : 0.0f);
}
