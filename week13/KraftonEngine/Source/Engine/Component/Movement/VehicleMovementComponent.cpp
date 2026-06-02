#include "VehicleMovementComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/WheelMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Input/InputSystem.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/PhysXPhysicsScene.h"
#include "Physics/PhysXTypeConversions.h"

#include <algorithm>
#include <array>
#include <vector>

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

	void DrawDebugPhysXBox(UWorld* World, const PxTransform& WorldPose, const PxBoxGeometry& Box, const FColor& Color)
	{
		const PxVec3& E = Box.halfExtents;
		const PxVec3 LocalCorners[8] = {
			PxVec3(-E.x, -E.y, -E.z), PxVec3(E.x, -E.y, -E.z),
			PxVec3(E.x, E.y, -E.z), PxVec3(-E.x, E.y, -E.z),
			PxVec3(-E.x, -E.y, E.z), PxVec3(E.x, -E.y, E.z),
			PxVec3(E.x, E.y, E.z), PxVec3(-E.x, E.y, E.z),
		};

		FVector WorldCorners[8];
		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			WorldCorners[CornerIndex] = ToFVector(WorldPose.transform(LocalCorners[CornerIndex]));
		}

		DrawDebugBox(
			World,
			WorldCorners[0], WorldCorners[1], WorldCorners[2], WorldCorners[3],
			WorldCorners[4], WorldCorners[5], WorldCorners[6], WorldCorners[7],
			Color);
	}

	void DrawDebugPhysXCapsule(UWorld* World, const PxTransform& WorldPose, const PxCapsuleGeometry& Capsule, const FColor& Color)
	{
		constexpr int32 SegmentCount = 12;
		const float AngleStep = PxTwoPi / static_cast<float>(SegmentCount);

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const float Angle0 = AngleStep * static_cast<float>(SegmentIndex);
			const float Angle1 = AngleStep * static_cast<float>(SegmentIndex + 1);
			const PxVec3 RingOffset0(0.0f, std::cos(Angle0) * Capsule.radius, std::sin(Angle0) * Capsule.radius);
			const PxVec3 RingOffset1(0.0f, std::cos(Angle1) * Capsule.radius, std::sin(Angle1) * Capsule.radius);

			const FVector Negative0 = ToFVector(WorldPose.transform(PxVec3(-Capsule.halfHeight, 0.0f, 0.0f) + RingOffset0));
			const FVector Negative1 = ToFVector(WorldPose.transform(PxVec3(-Capsule.halfHeight, 0.0f, 0.0f) + RingOffset1));
			const FVector Positive0 = ToFVector(WorldPose.transform(PxVec3(Capsule.halfHeight, 0.0f, 0.0f) + RingOffset0));
			const FVector Positive1 = ToFVector(WorldPose.transform(PxVec3(Capsule.halfHeight, 0.0f, 0.0f) + RingOffset1));

			DrawDebugLine(World, Negative0, Negative1, Color);
			DrawDebugLine(World, Positive0, Positive1, Color);
			DrawDebugLine(World, Negative0, Positive0, Color);
		}

		DrawDebugSphere(World, ToFVector(WorldPose.transform(PxVec3(-Capsule.halfHeight, 0.0f, 0.0f))), Capsule.radius, SegmentCount, Color);
		DrawDebugSphere(World, ToFVector(WorldPose.transform(PxVec3(Capsule.halfHeight, 0.0f, 0.0f))), Capsule.radius, SegmentCount, Color);
	}

	void DrawDebugWheelEnvelope(UWorld* World, const PxTransform& WorldPose, float Radius, float Width, const FColor& Color)
	{
		constexpr int32 SegmentCount = 16;
		const float HalfWidth = Width * 0.5f;
		const float AngleStep = PxTwoPi / static_cast<float>(SegmentCount);

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const float Angle0 = AngleStep * static_cast<float>(SegmentIndex);
			const float Angle1 = AngleStep * static_cast<float>(SegmentIndex + 1);
			const PxVec3 Negative0(std::cos(Angle0) * Radius, -HalfWidth, std::sin(Angle0) * Radius);
			const PxVec3 Negative1(std::cos(Angle1) * Radius, -HalfWidth, std::sin(Angle1) * Radius);
			const PxVec3 Positive0(std::cos(Angle0) * Radius, HalfWidth, std::sin(Angle0) * Radius);
			const PxVec3 Positive1(std::cos(Angle1) * Radius, HalfWidth, std::sin(Angle1) * Radius);

			DrawDebugLine(World, ToFVector(WorldPose.transform(Negative0)), ToFVector(WorldPose.transform(Negative1)), Color);
			DrawDebugLine(World, ToFVector(WorldPose.transform(Positive0)), ToFVector(WorldPose.transform(Positive1)), Color);
			if (SegmentIndex % 4 == 0)
			{
				DrawDebugLine(World, ToFVector(WorldPose.transform(Negative0)), ToFVector(WorldPose.transform(Positive0)), Color);
			}
		}
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
	DrawDebugVehicle();
}

void UVehicleMovementComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);
}

void UVehicleMovementComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObjects(WheelSceneComponents, "UVehicleMovementComponent.WheelSceneComponents");
	Collector.AddReferencedObjects(WheelMeshComponents, "UVehicleMovementComponent.WheelMeshComponents");
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

void UVehicleMovementComponent::SetWheelMeshComponents(const TArray<UWheelMeshComponent*>& InWheelMeshComponents)
{
	WheelMeshComponents.clear();
	WheelMeshComponents.reserve(InWheelMeshComponents.size());

	for (UWheelMeshComponent* WheelMeshComponent : InWheelMeshComponents)
	{
		WheelMeshComponents.push_back(WheelMeshComponent);
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

	PxVec3 SuspensionTravelDirectionPx = ToPxVec3(SuspensionTravelDirection);
	if (SuspensionTravelDirectionPx.magnitudeSquared() <= 0.000001f)
	{
		SuspensionTravelDirectionPx = PxVec3(0.0f, 0.0f, -1.0f);
	}
	else
	{
		SuspensionTravelDirectionPx.normalize();
	}

	for (PxU32 WheelIndex = 0; WheelIndex < VehicleWheelCount; ++WheelIndex)
	{
		PxVehicleWheelData Wheel;
		Wheel.mRadius = WheelRadius;
		Wheel.mWidth = WheelWidth;
		Wheel.mMass = WheelMass;
		Wheel.mMOI = WheelMomentOfInertiaScale * Wheel.mMass * Wheel.mRadius * Wheel.mRadius;
		Wheel.mMaxSteer = WheelIndex < 2 ? PxPi * FrontWheelMaxSteerDegrees / 180.0f : 0.0f;
		Wheel.mMaxHandBrakeTorque = WheelIndex >= 2 ? RearWheelMaxHandBrakeTorque : 0.0f;

		PxVehicleSuspensionData Suspension;
		Suspension.mMaxCompression = SuspensionMaxCompression;
		Suspension.mMaxDroop = SuspensionMaxDroop;
		Suspension.mSpringStrength = SuspensionSpringStrength;
		Suspension.mSpringDamperRate = SuspensionSpringDamperRate;
		Suspension.mSprungMass = SprungMasses[WheelIndex];

		PxVehicleTireData Tire;
		Tire.mType = static_cast<PxU32>(std::max(TireType, 0));

		PxFilterData SceneQueryFilterData;
		SceneQueryFilterData.word3 = OwnerActor->GetUUID();

		WheelsSimData->setWheelData(WheelIndex, Wheel);
		WheelsSimData->setSuspensionData(WheelIndex, Suspension);
		WheelsSimData->setTireData(WheelIndex, Tire);
		WheelsSimData->setSuspTravelDirection(WheelIndex, SuspensionTravelDirectionPx);
		WheelsSimData->setWheelCentreOffset(WheelIndex, WheelOffsets[WheelIndex]);
		WheelsSimData->setSuspForceAppPointOffset(WheelIndex, PxVec3(WheelOffsets[WheelIndex].x, WheelOffsets[WheelIndex].y, SuspensionForceApplicationOffsetZ));
		WheelsSimData->setTireForceAppPointOffset(WheelIndex, PxVec3(WheelOffsets[WheelIndex].x, WheelOffsets[WheelIndex].y, TireForceApplicationOffsetZ));
		WheelsSimData->setSceneQueryFilterData(WheelIndex, SceneQueryFilterData);
		WheelsSimData->setWheelShapeMapping(WheelIndex, -1);
	}

	PxVehicleDriveSimData4W DriveSimData;

	PxVehicleEngineData Engine;
	Engine.mPeakTorque = EnginePeakTorque;
	Engine.mMaxOmega = EngineMaxOmega;
	Engine.mTorqueCurve.clear();
	std::array<PxVec2, 4> TorqueCurvePairs = {
		PxVec2(EngineTorqueCurve0Omega, EngineTorqueCurve0Torque),
		PxVec2(EngineTorqueCurve1Omega, EngineTorqueCurve1Torque),
		PxVec2(EngineTorqueCurve2Omega, EngineTorqueCurve2Torque),
		PxVec2(EngineTorqueCurve3Omega, EngineTorqueCurve3Torque),
	};
	std::sort(TorqueCurvePairs.begin(), TorqueCurvePairs.end(), [](const PxVec2& Left, const PxVec2& Right)
	{
		return Left.x < Right.x;
	});
	for (const PxVec2& TorqueCurvePair : TorqueCurvePairs)
	{
		Engine.mTorqueCurve.addPair(TorqueCurvePair.x, TorqueCurvePair.y);
	}
	DriveSimData.setEngineData(Engine);

	PxVehicleGearsData Gears;
	Gears.mNbRatios = static_cast<PxU32>(std::clamp(GearRatioCount, 3, 7));
	DriveSimData.setGearsData(Gears);

	PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = AckermannAccuracy;
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
	VehicleDrive->mDriveDynData.setUseAutoGears(bUseAutoGears);

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
	FrictionPairs->setTypePairFriction(0, 0, SurfaceFriction);

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
	RawInput.setDigitalHandbrake(Input.GetKey(VK_SPACE));

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

	const size_t MeshCount = std::min<size_t>(WheelMeshComponents.size(), VehicleWheelCount);
	for (size_t WheelIndex = 0; WheelIndex < MeshCount; ++WheelIndex)
	{
		UWheelMeshComponent* WheelMeshComponent = WheelMeshComponents[WheelIndex].GetValid();
		if (!WheelMeshComponent)
		{
			continue;
		}

		const PxWheelQueryResult& Query = WheelQueryResults[WheelIndex];
		WheelMeshComponent->SetSuspensionLoad(Query.suspSpringForce, ToFVector(Query.tireContactNormal), WheelRadius, Query.isInAir);
	}
}

void UVehicleMovementComponent::DrawDebugVehicle() const
{
	if (!bDebugVisualizationVisible)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* RootPrimitive = OwnerActor ? Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()) : nullptr;
	if (!World || !RootPrimitive)
	{
		return;
	}

	PxRigidDynamic* ChassisActor = RootPrimitive->GetBodyInstance().GetRigidDynamic();
	if (!ChassisActor)
	{
		return;
	}

	const PxU32 ShapeCount = ChassisActor->getNbShapes();
	std::vector<PxShape*> ChassisShapes(ShapeCount);
	ChassisActor->getShapes(ChassisShapes.data(), ShapeCount);
	for (PxShape* Shape : ChassisShapes)
	{
		if (!Shape || !(Shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE))
		{
			continue;
		}

		const PxGeometryHolder Geometry = Shape->getGeometry();
		const PxTransform ShapeWorldPose = ChassisActor->getGlobalPose() * Shape->getLocalPose();
		switch (Geometry.getType())
		{
		case PxGeometryType::eBOX:
			DrawDebugPhysXBox(World, ShapeWorldPose, Geometry.box(), FColor(80, 160, 255));
			break;
		case PxGeometryType::eSPHERE:
			DrawDebugSphere(World, ToFVector(ShapeWorldPose.p), Geometry.sphere().radius, 16, FColor(80, 160, 255));
			break;
		case PxGeometryType::eCAPSULE:
			DrawDebugPhysXCapsule(World, ShapeWorldPose, Geometry.capsule(), FColor(80, 160, 255));
			break;
		default:
			break;
		}
	}

	const size_t Count = std::min<size_t>(WheelSceneComponents.size(), VehicleWheelCount);
	for (size_t WheelIndex = 0; WheelIndex < Count; ++WheelIndex)
	{
		const PxWheelQueryResult& Query = WheelQueryResults[WheelIndex];
		const FVector RayStart = ToFVector(Query.suspLineStart);
		const FVector RayEnd = RayStart + ToFVector(Query.suspLineDir) * Query.suspLineLength;
		DrawDebugLine(World, RayStart, RayEnd, Query.isInAir ? FColor(255, 80, 80) : FColor(80, 255, 80));

		const PxTransform WheelWorldPose = ChassisActor->getGlobalPose() * Query.localPose;
		DrawDebugWheelEnvelope(World, WheelWorldPose, WheelRadius, WheelWidth, FColor(255, 220, 80));

		if (!Query.isInAir)
		{
			const FVector ContactPoint = ToFVector(Query.tireContactPoint);
			DrawDebugPoint(World, ContactPoint, 0.08f, FColor(255, 80, 80));
			DrawDebugLine(World, ContactPoint, ContactPoint + ToFVector(Query.tireContactNormal) * 0.35f, FColor(80, 255, 255));
		}
	}
}
