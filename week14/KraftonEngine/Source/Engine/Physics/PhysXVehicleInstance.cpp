#include "Physics/PhysXVehicleInstance.h"
#include "Physics/PhysXConversions.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/MathUtils.h"

bool FPhysXVehicleInstance::Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose, const FVehiclePhysicsSetup& Setup)
{
	constexpr physx::PxU32 NumWheels = 4;

	const FVehicleWheelSetup* WheelSetups[NumWheels] =
	{
		&Setup.FrontLeftWheel,
		&Setup.FrontRightWheel,
		&Setup.RearLeftWheel,
		&Setup.RearRightWheel
	};

	const float ChassisMass = Setup.ChassisMass;
	const physx::PxVec3 ChassisDims = physx::PxVec3(Setup.ChassisHalfExtent.X, Setup.ChassisHalfExtent.Y, Setup.ChassisHalfExtent.Z);

	VehicleActor = Physics->createRigidDynamic(StartPose);
	if (!VehicleActor) return false;

	VehicleActor->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);

	for (physx::PxU32 Index = 0; Index < NumWheels; ++Index)
	{
		physx::PxShape* WheelShape = Physics->createShape(physx::PxSphereGeometry(WheelSetups[Index]->Radius), *Material, true);

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

	physx::PxTransform CenterOfMassOffset(physx::PxIdentity);
	CenterOfMassOffset.p = ToPxVec3(Setup.CenterOfMassOffset);
	VehicleActor->setCMassLocalPose(CenterOfMassOffset);

	physx::PxVehicleWheelsSimData* WheelsSimData = physx::PxVehicleWheelsSimData::allocate(NumWheels);

	physx::PxVec3 WheelCentreOffsets[NumWheels];
	for (physx::PxU32 Index = 0; Index < NumWheels; ++Index)
	{
		WheelCentreOffsets[Index] = ToPxVec3(WheelSetups[Index]->Offset);
	}

	physx::PxF32 SprungMasses[NumWheels];

	physx::PxVehicleComputeSprungMasses(NumWheels, WheelCentreOffsets, ToPxVec3(Setup.CenterOfMassOffset), ChassisMass, 2, SprungMasses);

	for (physx::PxU32 Index = 0; Index < NumWheels; ++Index)
	{
		physx::PxVehicleWheelData Wheel;
		Wheel.mMass = WheelSetups[Index]->Mass;
		Wheel.mRadius = WheelSetups[Index]->Radius;
		Wheel.mWidth = WheelSetups[Index]->Width;
		Wheel.mMOI = 0.5f * WheelSetups[Index]->Mass * WheelSetups[Index]->Radius * WheelSetups[Index]->Radius;
		Wheel.mDampingRate = WheelSetups[Index]->DampingRate;
		Wheel.mMaxSteer = WheelSetups[Index]->MaxSteer;
		Wheel.mMaxBrakeTorque = WheelSetups[Index]->MaxBrakeTorque;
		Wheel.mMaxHandBrakeTorque = WheelSetups[Index]->MaxHandBrakeTorque;

		WheelsSimData->setWheelData(Index, Wheel);

		physx::PxVehicleSuspensionData Suspension;
		Suspension.mMaxCompression = 0.3f;
		Suspension.mMaxDroop = 0.1f;
		Suspension.mSpringStrength = 35000.0f;
		Suspension.mSpringDamperRate = 4500.0f;
		Suspension.mSprungMass = SprungMasses[Index];
		WheelsSimData->setSuspensionData(Index, Suspension);

		physx::PxVehicleTireData Tire;
		Tire.mType = 0;
		WheelsSimData->setTireData(Index, Tire);

		const physx::PxVec3 WheelOffset = WheelCentreOffsets[Index];

		const physx::PxVec3 ForceAppPointOffset(WheelOffset.x, WheelOffset.y, -0.3f);

		WheelsSimData->setWheelCentreOffset(Index, WheelCentreOffsets[Index]);
		WheelsSimData->setSuspForceAppPointOffset(Index, ForceAppPointOffset);
		WheelsSimData->setTireForceAppPointOffset(Index, ForceAppPointOffset);
		WheelsSimData->setSuspTravelDirection(Index, physx::PxVec3(0, 0, -1));
		WheelsSimData->setWheelShapeMapping(Index, Index);

		physx::PxFilterData SuspensionQueryFilterData;
		SuspensionQueryFilterData.word0 = ObjectTypeBit(ECollisionChannel::WorldStatic);
		WheelsSimData->setSceneQueryFilterData(Index, SuspensionQueryFilterData);
	}

	physx::PxVehicleDriveSimData4W DriveData;

	physx::PxVehicleEngineData Engine;
	Engine.mPeakTorque = Setup.EnginePeakTorque;
	Engine.mMaxOmega = Setup.EngineMaxOmega;
	DriveData.setEngineData(Engine);

	physx::PxVehicleGearsData Gears;
	DriveData.setGearsData(Gears);

	physx::PxVehicleClutchData Clutch;
	Clutch.mStrength = Setup.ClutchStrength;
	DriveData.setClutchData(Clutch);

	physx::PxVehicleDifferential4WData Diff;
	Diff.mType = physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
	DriveData.setDiffData(Diff);
	
	physx::PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = 1.0f;
	Ackermann.mAxleSeparation = 3.0f;
	Ackermann.mFrontWidth = 2.0f;
	Ackermann.mRearWidth = 2.0f;
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

void FPhysXVehicleInstance::SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse)
{
	if (!Vehicle || !VehicleActor) return;

	const physx::PxTransform ActorPose = VehicleActor->getGlobalPose();
	const physx::PxVec3 Forward = ActorPose.q.rotate(physx::PxVec3(1, 0, 0));
	const float ForwardSpeed = VehicleActor->getLinearVelocity().dot(Forward);

	constexpr float GearSwitchSpeedThreshold = 0.5f;
	const bool bCanSwitchGear = std::abs(ForwardSpeed) < GearSwitchSpeedThreshold;

	float AppliedThrottle = FMath::Clamp(Throttle, -1.0f, 1.0f);
	float AppliedBrake = FMath::Clamp(Brake, 0.0f, 1.0f);

	if (bReverse && !bIsReverse)
	{
		if (bCanSwitchGear)
		{
			Vehicle->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eREVERSE);
			bIsReverse = true;
		}
		else
		{
			AppliedThrottle = 0.0f;
			AppliedBrake = 1.0f;
		}
	}
	else if (!bReverse && bIsReverse)
	{
		if (bCanSwitchGear)
		{
			Vehicle->mDriveDynData.forceGearChange(physx::PxVehicleGearsData::eFIRST);
			bIsReverse = false;
		}
		else
		{
			AppliedThrottle = 0.0f;
			AppliedBrake = 1.0f;
		}
	}

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL,
		AppliedThrottle);

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE,
		AppliedBrake);

	const float ClampedSteer = FMath::Clamp(Steer, -1.0f, 1.0f);

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT,
		ClampedSteer < 0.0f ? -ClampedSteer : 0.0f);

	Vehicle->mDriveDynData.setAnalogInput(physx::PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT,
		ClampedSteer > 0.0f ? ClampedSteer : 0.0f);
}
