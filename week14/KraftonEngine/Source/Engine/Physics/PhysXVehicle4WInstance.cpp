#include "Physics/PhysXVehicle4WInstance.h"

#include "Physics/PhysicsFilterData.h"
#include "Physics/PhysXConversions.h"

#include "Core/Types/CollisionTypes.h"
#include "Math/MathUtils.h"

namespace
{
	struct FResolvedVehicleCollisionSettings
	{
		ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
		ECollisionChannel ObjectType = ECollisionChannel::WorldDynamic;
		FCollisionResponseContainer ResponseContainer;
		bool bGenerateOverlapEvents = false;
	};

	FResolvedVehicleCollisionSettings ResolveVehicleCollisionSettings(const FVehiclePhysicsSetup& Setup)
	{
		FResolvedVehicleCollisionSettings Settings;

		if (Setup.CollisionPreset == ECollisionPreset::Custom)
		{
			Settings.CollisionEnabled = Setup.CollisionEnabled;
			Settings.ObjectType = Setup.ObjectType;
			Settings.ResponseContainer = Setup.ResponseContainer;
			Settings.bGenerateOverlapEvents = Setup.bGenerateOverlapEvents;
			return Settings;
		}

		switch (Setup.CollisionPreset)
		{
		case ECollisionPreset::NoCollision:
			Settings.CollisionEnabled = ECollisionEnabled::NoCollision;
			Settings.ObjectType = ECollisionChannel::WorldStatic;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Ignore);
			Settings.bGenerateOverlapEvents = false;
			break;

		case ECollisionPreset::BlockAll:
			Settings.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
			Settings.ObjectType = ECollisionChannel::WorldStatic;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Block);
			Settings.bGenerateOverlapEvents = false;
			break;

		case ECollisionPreset::OverlapAll:
			Settings.CollisionEnabled = ECollisionEnabled::QueryOnly;
			Settings.ObjectType = ECollisionChannel::WorldDynamic;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Overlap);
			Settings.bGenerateOverlapEvents = true;
			break;

		case ECollisionPreset::WorldStatic:
			Settings.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
			Settings.ObjectType = ECollisionChannel::WorldStatic;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Block);
			Settings.bGenerateOverlapEvents = false;
			break;

		case ECollisionPreset::WorldDynamic:
		case ECollisionPreset::PhysicsActor:
			Settings.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
			Settings.ObjectType = ECollisionChannel::WorldDynamic;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Block);
			Settings.bGenerateOverlapEvents = false;
			break;

		case ECollisionPreset::Trigger:
			Settings.CollisionEnabled = ECollisionEnabled::QueryOnly;
			Settings.ObjectType = ECollisionChannel::Trigger;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Overlap);
			Settings.bGenerateOverlapEvents = true;
			break;

		case ECollisionPreset::Pawn:
			Settings.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
			Settings.ObjectType = ECollisionChannel::Pawn;
			Settings.ResponseContainer.SetAllChannels(ECollisionResponse::Block);
			Settings.ResponseContainer.SetResponse(ECollisionChannel::Trigger, ECollisionResponse::Overlap);
			Settings.bGenerateOverlapEvents = true;
			break;

		case ECollisionPreset::Custom:
		default:
			break;
		}

		return Settings;
	}

	void ApplyVehicleCollisionToShape(physx::PxShape& Shape, const FVehiclePhysicsSetup& Setup)
	{
		const FResolvedVehicleCollisionSettings CollisionSettings = ResolveVehicleCollisionSettings(Setup);
		const physx::PxFilterData FilterData = MakeFilterData(
			CollisionSettings.ObjectType,
			CollisionSettings.ResponseContainer,
			CollisionSettings.CollisionEnabled,
			CollisionSettings.bGenerateOverlapEvents);

		Shape.setSimulationFilterData(FilterData);
		Shape.setQueryFilterData(FilterData);

		switch (CollisionSettings.CollisionEnabled)
		{
		case ECollisionEnabled::NoCollision:
			Shape.setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
			Shape.setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
			Shape.setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
			break;

		case ECollisionEnabled::QueryOnly:
			Shape.setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
			Shape.setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
			Shape.setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
			break;

		case ECollisionEnabled::PhysicsOnly:
			Shape.setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
			Shape.setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
			Shape.setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
			break;

		case ECollisionEnabled::QueryAndPhysics:
			Shape.setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
			Shape.setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
			Shape.setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
			break;
		}
	}

	physx::PxVehicleDifferential4WData::Enum ToPxDifferentialType(EVehicleDifferential4WType Type)
	{
		switch (Type)
		{
		case EVehicleDifferential4WType::LimitedSlipFrontDrive:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_FRONTWD;
		case EVehicleDifferential4WType::LimitedSlipRearDrive:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_REARWD;
		case EVehicleDifferential4WType::Open4W:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;
		case EVehicleDifferential4WType::OpenFrontDrive:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_FRONTWD;
		case EVehicleDifferential4WType::OpenRearDrive:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_REARWD;
		case EVehicleDifferential4WType::LimitedSlip4W:
		default:
			return physx::PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
		}
	}

	void ComputeAckermannGeometryFromWheelOffsets(const physx::PxVec3 WheelCentreOffsets[4],
		float& OutAxleSeparation, float& OutFrontWidth, float& OutRearWidth)
	{
		const physx::PxVec3& FrontLeft = WheelCentreOffsets[physx::PxVehicleDrive4WWheelOrder::eFRONT_LEFT];
		const physx::PxVec3& FrontRight = WheelCentreOffsets[physx::PxVehicleDrive4WWheelOrder::eFRONT_RIGHT];
		const physx::PxVec3& RearLeft = WheelCentreOffsets[physx::PxVehicleDrive4WWheelOrder::eREAR_LEFT];
		const physx::PxVec3& RearRight = WheelCentreOffsets[physx::PxVehicleDrive4WWheelOrder::eREAR_RIGHT];

		const float FrontX = (FrontLeft.x + FrontRight.x) * 0.5f;
		const float RearX = (RearLeft.x + RearRight.x) * 0.5f;

		OutAxleSeparation = std::abs(FrontX - RearX);
		OutFrontWidth = std::abs(FrontLeft.y - FrontRight.y);
		OutRearWidth = std::abs(RearLeft.y - RearRight.y);
	}
}

bool FPhysXVehicle4WInstance::Initialize(physx::PxPhysics* Physics, physx::PxScene* Scene,
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
	if (!ChassisShape) return false;

	ApplyVehicleCollisionToShape(*ChassisShape, Setup);
	VehicleActor->attachShape(*ChassisShape);
	ChassisShape->release();

	physx::PxRigidBodyExt::setMassAndUpdateInertia(*VehicleActor, ChassisMass);
	VehicleActor->setLinearDamping(Setup.LinearDamping);
	VehicleActor->setAngularDamping(Setup.AngularDamping);

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
		Suspension.mMaxCompression = WheelSetups[Index]->SuspensionMaxCompression;
		Suspension.mMaxDroop = WheelSetups[Index]->SuspensionMaxDroop;
		Suspension.mSpringStrength = WheelSetups[Index]->SuspensionSpringStrength;
		Suspension.mSpringDamperRate = WheelSetups[Index]->SuspensionSpringDamperRate;
		Suspension.mSprungMass = SprungMasses[Index];
		WheelsSimData->setSuspensionData(Index, Suspension);

		physx::PxVehicleTireData Tire;
		Tire.mType = 0;
		WheelsSimData->setTireData(Index, Tire);

		const physx::PxVec3 WheelOffset = WheelCentreOffsets[Index];

		const physx::PxVec3 ForceAppPointOffset(WheelOffset.x, WheelOffset.y, Setup.ForceAppPointZOffset);

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
	Diff.mType = ToPxDifferentialType(Setup.DifferentialType);
	DriveData.setDiffData(Diff);

	physx::PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = Setup.AckermannAccuracy;
	Ackermann.mAxleSeparation = Setup.AckermannAxleSeparation;
	Ackermann.mFrontWidth = Setup.AckermannFrontWidth;
	Ackermann.mRearWidth = Setup.AckermannRearWidth;
	if (Setup.bAutoAckermannFromWheelOffsets)
	{
		ComputeAckermannGeometryFromWheelOffsets(WheelCentreOffsets,
			Ackermann.mAxleSeparation, Ackermann.mFrontWidth, Ackermann.mRearWidth);
	}
	DriveData.setAckermannGeometryData(Ackermann);

	Vehicle = physx::PxVehicleDrive4W::allocate(NumWheels);
	Vehicle->setup(Physics, VehicleActor, *WheelsSimData, DriveData, NumWheels - 4);
	Vehicle->setToRestState();
	Vehicle->mDriveDynData.setUseAutoGears(Setup.bUseAutoGears);

	WheelsSimData->free();

	Scene->addActor(*VehicleActor);
	return true;
}

void FPhysXVehicle4WInstance::Shutdown()
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

void FPhysXVehicle4WInstance::SetDriveInput(float Throttle, float Brake, float Steer, bool bReverse)
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
