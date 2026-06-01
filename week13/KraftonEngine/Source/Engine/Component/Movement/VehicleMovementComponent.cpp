#include "VehicleMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Input/InputSystem.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/PhysXPhysicsScene.h"
#include "Physics/PhysXTypeConversions.h"

#include <algorithm>

using namespace physx;
using namespace PhysXConvert;

namespace
{
	constexpr PxU32 VehicleWheelCount = UVehicleMovementComponent::WheelCount;

	PxQueryHitType::Enum SuspensionPreFilter(
		PxFilterData QueryFilterData,
		PxFilterData ShapeFilterData,
		const void*,
		PxU32,
		PxHitFlags&)
	{
		if (QueryFilterData.word3 != 0 && QueryFilterData.word3 == ShapeFilterData.word3)
		{
			return PxQueryHitType::eNONE;
		}
		return PxQueryHitType::eBLOCK;
	}

	const PxVehicleKeySmoothingData& GetKeySmoothingData()
	{
		static const PxVehicleKeySmoothingData Data = {
			{ 6.0f, 6.0f, 12.0f, 2.5f, 2.5f },
			{ 10.0f, 10.0f, 12.0f, 5.0f, 5.0f },
		};
		return Data;
	}

	const PxFixedSizeLookupTable<8>& GetSteerVsForwardSpeedTable()
	{
		static const PxFixedSizeLookupTable<8> Table = []()
		{
			PxFixedSizeLookupTable<8> Result;
			Result.addPair(0.0f, 0.75f);
			Result.addPair(5.0f, 0.75f);
			Result.addPair(15.0f, 0.5f);
			Result.addPair(30.0f, 0.25f);
			return Result;
		}();
		return Table;
	}
}

void UVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeVehicle();
}

void UVehicleMovementComponent::EndPlay()
{
	ReleaseVehicle();
	Super::EndPlay();
}

void UVehicleMovementComponent::BeginDestroy()
{
	ReleaseVehicle();
	Super::BeginDestroy();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!VehicleDrive || !SuspensionBatchQuery || !FrictionPairs || DeltaTime <= 0.0f)
	{
		return;
	}

	ApplyKeyboardInput(DeltaTime);

	PxVehicleWheels* Vehicles[] = { VehicleDrive };
	PxVehicleSuspensionRaycasts(
		SuspensionBatchQuery,
		1,
		Vehicles,
		VehicleWheelCount,
		SuspensionRaycastResults);

	PxVehicleUpdates(
		DeltaTime,
		PxVec3(0.0f, 0.0f, -9.81f),
		*FrictionPairs,
		1,
		Vehicles,
		&VehicleWheelQueryResult);

	UpdateWheelVisuals();
}

void UVehicleMovementComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
}

void UVehicleMovementComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObjects(WheelSceneComponents, "UVehicleMovementComponent.WheelSceneComponents");
}

void UVehicleMovementComponent::SetWheelSceneComponents(const TArray<USceneComponent*>& InWheelSceneComponents)
{
	WheelSceneComponents.clear();
	WheelSceneComponents.reserve(InWheelSceneComponents.size());

	for (USceneComponent* WheelComponent : InWheelSceneComponents)
	{
		WheelSceneComponents.push_back(WheelComponent);
	}
}

bool UVehicleMovementComponent::InitializeVehicle()
{
	ReleaseVehicle();

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* RootPrimitive = OwnerActor ? Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()) : nullptr;
	if (!World || !RootPrimitive)
	{
		return false;
	}

	FPhysXPhysicsScene* PhysicsScene = static_cast<FPhysXPhysicsScene*>(World->GetPhysicsScene());
	PxScene* PxScene = PhysicsScene ? PhysicsScene->GetPxScene() : nullptr;
	PxMaterial* DefaultMaterial = PhysicsScene ? PhysicsScene->GetDefaultMaterial() : nullptr;
	if (!PxScene || !DefaultMaterial)
	{
		return false;
	}

	PxRigidDynamic* BodyActor = RootPrimitive->GetBodyInstance().GetRigidDynamic();
	if (!BodyActor)
	{
		return false;
	}

	if (WheelSceneComponents.size() != VehicleWheelCount)
	{
		return false;
	}

	PxVec3 WheelOffsets[VehicleWheelCount];
	for (PxU32 WheelIndex = 0; WheelIndex < VehicleWheelCount; ++WheelIndex)
	{
		USceneComponent* WheelComponent = WheelSceneComponents[WheelIndex].GetValid();
		if (!WheelComponent)
		{
			return false;
		}

		WheelOffsets[WheelIndex] = ToPxVec3(WheelComponent->GetRelativeLocation());
	}

	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(VehicleWheelCount);
	if (!WheelsSimData)
	{
		return false;
	}

	PxReal SprungMasses[VehicleWheelCount];
	PxVehicleComputeSprungMasses(
		VehicleWheelCount,
		WheelOffsets,
		BodyActor->getCMassLocalPose().p,
		BodyActor->getMass(),
		2,
		SprungMasses);

	for (PxU32 WheelIndex = 0; WheelIndex < VehicleWheelCount; ++WheelIndex)
	{
		PxVehicleWheelData Wheel;
		Wheel.mRadius = 0.38f;
		Wheel.mWidth = 0.3f;
		Wheel.mMass = 20.0f;
		Wheel.mMOI = 0.5f * Wheel.mMass * Wheel.mRadius * Wheel.mRadius;
		Wheel.mMaxSteer = WheelIndex < 2 ? PxPi * 0.25f : 0.0f;
		Wheel.mMaxHandBrakeTorque = WheelIndex >= 2 ? 4000.0f : 0.0f;

		PxVehicleSuspensionData Suspension;
		Suspension.mMaxCompression = 0.3f;
		Suspension.mMaxDroop = 0.1f;
		Suspension.mSpringStrength = 35000.0f;
		Suspension.mSpringDamperRate = 4500.0f;
		Suspension.mSprungMass = SprungMasses[WheelIndex];

		PxVehicleTireData Tire;
		Tire.mType = 0;

		PxFilterData SceneQueryFilterData;
		SceneQueryFilterData.word3 = OwnerActor->GetUUID();

		WheelsSimData->setWheelData(WheelIndex, Wheel);
		WheelsSimData->setSuspensionData(WheelIndex, Suspension);
		WheelsSimData->setTireData(WheelIndex, Tire);
		WheelsSimData->setSuspTravelDirection(WheelIndex, PxVec3(0.0f, 0.0f, -1.0f));
		WheelsSimData->setWheelCentreOffset(WheelIndex, WheelOffsets[WheelIndex]);
		WheelsSimData->setSuspForceAppPointOffset(WheelIndex, PxVec3(WheelOffsets[WheelIndex].x, WheelOffsets[WheelIndex].y, -0.3f));
		WheelsSimData->setTireForceAppPointOffset(WheelIndex, PxVec3(WheelOffsets[WheelIndex].x, WheelOffsets[WheelIndex].y, -0.3f));
		WheelsSimData->setSceneQueryFilterData(WheelIndex, SceneQueryFilterData);
		WheelsSimData->setWheelShapeMapping(WheelIndex, -1);
	}

	PxVehicleDriveSimData4W DriveSimData;

	PxVehicleEngineData Engine;
	Engine.mPeakTorque = 500.0f;
	Engine.mMaxOmega = 600.0f;
	Engine.mTorqueCurve.addPair(0.0f, 0.2f);
	Engine.mTorqueCurve.addPair(0.2f, 0.6f);
	Engine.mTorqueCurve.addPair(0.5f, 1.0f);
	Engine.mTorqueCurve.addPair(1.0f, 0.2f);
	DriveSimData.setEngineData(Engine);

	PxVehicleGearsData Gears;
	Gears.mNbRatios = 7;
	DriveSimData.setGearsData(Gears);

	PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = 1.0f;
	Ackermann.mAxleSeparation = WheelOffsets[0].x - WheelOffsets[2].x;
	Ackermann.mFrontWidth = WheelOffsets[0].y - WheelOffsets[1].y;
	Ackermann.mRearWidth = WheelOffsets[2].y - WheelOffsets[3].y;
	DriveSimData.setAckermannGeometryData(Ackermann);

	VehicleDrive = PxVehicleDrive4W::allocate(VehicleWheelCount);
	if (!VehicleDrive)
	{
		WheelsSimData->free();
		return false;
	}

	VehicleDrive->setup(&PxGetPhysics(), BodyActor, *WheelsSimData, DriveSimData, 0);
	WheelsSimData->free();

	VehicleDrive->setToRestState();
	VehicleDrive->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
	VehicleDrive->mDriveDynData.setUseAutoGears(true);

	FrictionPairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);
	if (!FrictionPairs)
	{
		ReleaseVehicle();
		return false;
	}

	const PxMaterial* SurfaceMaterials[] = { DefaultMaterial };
	PxVehicleDrivableSurfaceType SurfaceTypes[1];
	SurfaceTypes[0].mType = 0;
	FrictionPairs->setup(1, 1, SurfaceMaterials, SurfaceTypes);
	FrictionPairs->setTypePairFriction(0, 0, 1.0f);

	PxBatchQueryDesc BatchQueryDesc(VehicleWheelCount, 0, 0);
	BatchQueryDesc.queryMemory.userRaycastResultBuffer = SuspensionRaycastResults;
	BatchQueryDesc.queryMemory.userRaycastTouchBuffer = SuspensionRaycastHits;
	BatchQueryDesc.queryMemory.raycastTouchBufferSize = VehicleWheelCount;
	BatchQueryDesc.preFilterShader = SuspensionPreFilter;
	SuspensionBatchQuery = PxScene->createBatchQuery(BatchQueryDesc);
	if (!SuspensionBatchQuery)
	{
		ReleaseVehicle();
		return false;
	}

	VehicleWheelQueryResult.wheelQueryResults = WheelQueryResults;
	VehicleWheelQueryResult.nbWheelQueryResults = VehicleWheelCount;
	return true;
}

void UVehicleMovementComponent::ReleaseVehicle()
{
	if (SuspensionBatchQuery)
	{
		SuspensionBatchQuery->release();
		SuspensionBatchQuery = nullptr;
	}

	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}

	if (VehicleDrive)
	{
		VehicleDrive->free();
		VehicleDrive = nullptr;
	}

	VehicleWheelQueryResult.wheelQueryResults = nullptr;
	VehicleWheelQueryResult.nbWheelQueryResults = 0;
}

void UVehicleMovementComponent::ApplyKeyboardInput(float DeltaTime)
{
	const InputSystem& Input = InputSystem::Get();
	const bool bForwardPressed = Input.GetKey('W');
	const bool bReversePressed = Input.GetKey('S');
	const float ForwardSpeed = VehicleDrive->computeForwardSpeed();

	PxVehicleDrive4WRawInputData RawInput;
	RawInput.setDigitalSteerLeft(Input.GetKey('A'));
	RawInput.setDigitalSteerRight(Input.GetKey('D'));

	if (bReversePressed && ForwardSpeed < 0.5f)
	{
		VehicleDrive->mDriveDynData.forceGearChange(PxVehicleGearsData::eREVERSE);
		RawInput.setDigitalAccel(true);
	}
	else if (bForwardPressed && ForwardSpeed > -0.5f)
	{
		if (VehicleDrive->mDriveDynData.getCurrentGear() == PxVehicleGearsData::eREVERSE)
		{
			VehicleDrive->mDriveDynData.forceGearChange(PxVehicleGearsData::eFIRST);
		}
		RawInput.setDigitalAccel(true);
	}
	else if (bForwardPressed || bReversePressed)
	{
		RawInput.setDigitalBrake(true);
	}

	PxVehicleDrive4WSmoothDigitalRawInputsAndSetAnalogInputs(
		GetKeySmoothingData(),
		GetSteerVsForwardSpeedTable(),
		RawInput,
		DeltaTime,
		PxVehicleIsInAir(VehicleWheelQueryResult),
		*VehicleDrive);
}

void UVehicleMovementComponent::UpdateWheelVisuals()
{
	const size_t Count = std::min<size_t>(WheelSceneComponents.size(), VehicleWheelCount);
	for (size_t WheelIndex = 0; WheelIndex < Count; ++WheelIndex)
	{
		USceneComponent* WheelComponent = WheelSceneComponents[WheelIndex].GetValid();
		if (!WheelComponent)
		{
			continue;
		}

		WheelComponent->SetRelativeTransform(ToFTransform(WheelQueryResults[WheelIndex].localPose));
	}
}
