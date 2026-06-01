#include "Physics/PhysicsScene.h"
#include "Physics/PhysXSDK.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Physics/PhysicsShape.h"
#include "Physics/PhysXConversions.h"
#include "Physics/PhysicsEventCallback.h"
#include "Physics/PhysicsFilterData.h"
#include "Physics/PhysicsFilterShader.h"
#include "Physics/PhysicsQueryFilter.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Physics/PhysXVehicleManager.h"

#include "Core/Logging/Log.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "Render/Scene/FScene.h"

#include <algorithm>

namespace
{
	FVector ToClothLocalPoint(const FMatrix& WorldToCloth, const physx::PxVec3& WorldPoint)
	{
		return WorldToCloth.TransformPositionWithW(FVector(WorldPoint.x, WorldPoint.y, WorldPoint.z));
	}

	void AppendSphereClothCollision(
		const physx::PxSphereGeometry& Sphere,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		FVector LocalCenter = ToClothLocalPoint(Params.WorldToCloth, ShapeWorldPose.p);

		OutData.Spheres.push_back(physx::PxVec4(
			LocalCenter.X,
			LocalCenter.Y,
			LocalCenter.Z,
			Sphere.radius));
	}

	static void AppendCapsuleClothCollision(
		const physx::PxCapsuleGeometry& Capsule,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		const uint32 BaseIndex = static_cast<uint32>(OutData.Spheres.size());

		const physx::PxVec3 LocalA(Capsule.halfHeight, 0.0f, 0.0f);
		const physx::PxVec3 LocalB(-Capsule.halfHeight, 0.0f, 0.0f);

		const physx::PxVec3 WorldA = ShapeWorldPose.transform(LocalA);
		const physx::PxVec3 WorldB = ShapeWorldPose.transform(LocalB);

		FVector ClothA = ToClothLocalPoint(Params.WorldToCloth, WorldA);
		FVector ClothB = ToClothLocalPoint(Params.WorldToCloth, WorldB);

		OutData.Spheres.push_back(physx::PxVec4(ClothA.X, ClothA.Y, ClothA.Z, Capsule.radius));
		OutData.Spheres.push_back(physx::PxVec4(ClothB.X, ClothB.Y, ClothB.Z, Capsule.radius));

		OutData.Capsules.push_back(BaseIndex);
		OutData.Capsules.push_back(BaseIndex + 1);
	}

	void AppendOverlapHit(const physx::PxOverlapHit& PxHit, TArray<FOverlapResult>& OutOverlaps)
	{
		UPrimitiveComponent* Component = GetComponentFromQueryShape(PxHit.shape);
		if (!Component) return;

		FOverlapResult Result;
		Result.OverlapComponent = Component;
		Result.OverlapActor = Component->GetOwner();
		OutOverlaps.push_back(Result);
	}

	FBoundingBox ExpandBounds(const FBoundingBox& Bounds, float Padding)
	{
		if (!Bounds.IsValid() || Padding <= 0.0f)
		{
			return Bounds;
		}

		const FVector Extent(Padding, Padding, Padding);
		return FBoundingBox(Bounds.Min - Extent, Bounds.Max + Extent);
	}

	FVector ToClothLocalVector(const FMatrix& WorldToCloth, const physx::PxVec3& WorldVector)
	{
		FVector V = WorldToCloth.TransformVector(FVector(WorldVector.x, WorldVector.y, WorldVector.z));
		V.Normalize();
		return V;
	}

	void AppendPlaneFromWorldPointNormal(
		const physx::PxVec3& WorldPoint,
		const physx::PxVec3& WorldNormal,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		FVector LocalPoint = ToClothLocalPoint(Params.WorldToCloth, WorldPoint);
		FVector LocalNormal = ToClothLocalVector(Params.WorldToCloth, WorldNormal);

		const float D = -LocalNormal.Dot(LocalPoint);

		OutData.Planes.push_back(physx::PxVec4(
			LocalNormal.X,
			LocalNormal.Y,
			LocalNormal.Z,
			D));
	}

	void AppendBoxClothCollision(
		const physx::PxBoxGeometry& Box,
		const physx::PxTransform& ShapeWorldPose,
		const FClothCollisionGatherParams& Params,
		FClothCollisionData& OutData)
	{
		const uint32 PlaneBaseIndex = static_cast<uint32>(OutData.Planes.size());

		const physx::PxVec3 Extents = Box.halfExtents;

		struct FBoxPlane
		{
			physx::PxVec3 LocalNormal;
			physx::PxVec3 LocalPoint;
		};

		const FBoxPlane Planes[6] =
		{
			{ physx::PxVec3(1.0f,  0.0f,  0.0f), physx::PxVec3(Extents.x, 0.0f, 0.0f) },
			{ physx::PxVec3(-1.0f,  0.0f,  0.0f), physx::PxVec3(-Extents.x, 0.0f, 0.0f) },
			{ physx::PxVec3(0.0f,  1.0f,  0.0f), physx::PxVec3(0.0f,  Extents.y, 0.0f) },
			{ physx::PxVec3(0.0f, -1.0f,  0.0f), physx::PxVec3(0.0f, -Extents.y, 0.0f) },
			{ physx::PxVec3(0.0f,  0.0f,  1.0f), physx::PxVec3(0.0f, 0.0f,  Extents.z) },
			{ physx::PxVec3(0.0f,  0.0f, -1.0f), physx::PxVec3(0.0f, 0.0f, -Extents.z) },
		};

		for (const FBoxPlane& Plane : Planes)
		{
			const physx::PxVec3 WorldPoint = ShapeWorldPose.transform(Plane.LocalPoint);
			const physx::PxVec3 WorldNormal = ShapeWorldPose.rotate(Plane.LocalNormal);

			AppendPlaneFromWorldPointNormal(WorldPoint, WorldNormal, Params, OutData);
		}

		uint32 ConvexMask = 0;
		for (uint32 i = 0; i < 6; ++i)
		{
			ConvexMask |= 1u << (PlaneBaseIndex + i);
		}

		OutData.ConvexMasks.push_back(ConvexMask);
	}
}

void FPhysicsScene::Initialize()
{
	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();

	physx::PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = physx::PxVec3(0.0f, 0.0f, -9.8f);

	EventCallback = new FPhysicsEventCallback();
	SceneDesc.simulationEventCallback = EventCallback;

	Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = PhysicsFilterShader;
	SceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	SceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
	SceneDesc.flags |= physx::PxSceneFlag::eENABLE_PCM;

	Scene = Physics->createScene(SceneDesc);

	Scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
	Scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);

	VehicleManager = new FPhysXVehicleManager();
	VehicleManager->Initialize(Physics, Scene, FPhysXSDK::Get().GetDefaultMaterial());
}

void FPhysicsScene::Shutdown()
{
	if (VehicleManager)
	{
		VehicleManager->Shutdown();
		delete VehicleManager;
		VehicleManager = nullptr;
	}

	while (!Constraints.empty())
	{
		DestroyConstraint(Constraints.back());
	}

	while (!Bodies.empty())
	{
		FBodyInstance* Body = Bodies.back();
		if (Body)
		{
			DestroyBody(*Body);
		}
		else
		{
			Bodies.pop_back();
		}
	}

	if (Scene)
	{
		Scene->release();
		Scene = nullptr;
	}
	if (EventCallback)
	{
		delete EventCallback;
		EventCallback = nullptr;
	}
	if (Dispatcher)
	{
		Dispatcher->release();
		Dispatcher = nullptr;
	}
}

void FPhysicsScene::Simulate(float DeltaTime)
{
	if (VehicleManager)
	{
		VehicleManager->Update(DeltaTime);
	}

	if (Scene)
	{
		Scene->simulate(DeltaTime);
		Scene->fetchResults(true);

		for (FBodyInstance* Body : Bodies)
		{
			if (Body)
			{
				Body->SyncFromPhysics();
			}
		}
	}
}

bool FPhysicsScene::CreateBody(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance)
{
	if (!Scene || !OwnerComp) return false;

	if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OwnerComp))
	{
		UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
		UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;

		if (!BodySetup) return false;

		return CreateBodyFromSetup(OwnerComp, OutInstance, *BodySetup, OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion(),
			OwnerComp->GetCollisionObjectType(), OwnerComp->GetCollisionEnabled(), OwnerComp->GetWorldScale(),
			OwnerComp->GetGenerateOverlapEvents(), OwnerComp->IsSimulatingPhysics());
	}

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();
	physx::PxMaterial* DefaultMaterial = FPhysXSDK::Get().GetDefaultMaterial();

	OutInstance.OwnerComponent = OwnerComp;

	physx::PxRigidActor* Body = nullptr;

	if (OwnerComp->IsSimulatingPhysics())
	{
		Body = Physics->createRigidDynamic(ToPxTransform(OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion()));
		OutInstance.Mode = EBodyInstanceMode::Dynamic;
	}
	else
	{
		Body = Physics->createRigidStatic(ToPxTransform(OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion()));
		OutInstance.Mode = EBodyInstanceMode::Static;
	}

	const bool bTrigger = OwnerComp->GetCollisionEnabled() == ECollisionEnabled::QueryOnly;

	TArray<physx::PxShape*> Shapes;
	FPhysicsShapeFactory::CreateShapesForComponent(*Physics, *DefaultMaterial, OwnerComp, bTrigger, Shapes);

	if (Shapes.empty())
	{
		Body->release();

		OutInstance.Body = nullptr;
		OutInstance.OwnerComponent = nullptr;
		OutInstance.Mode = EBodyInstanceMode::Static;
		return false;
	}

	for (physx::PxShape* Shape : Shapes)
	{
		if (!Shape) continue;

		Body->attachShape(*Shape);
		Shape->release();
	}

	OutInstance.Body = Body;
	Body->userData = &OutInstance;

	Body->setActorFlag(physx::PxActorFlag::eVISUALIZATION, true);

	Scene->addActor(*Body);
	Bodies.push_back(&OutInstance);

	return true;
}

bool FPhysicsScene::CreateBodyFromSetup(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance, const UBodySetup& BodySetup,
	const FVector& WorldLocation, const FQuat& WorldRotation, ECollisionChannel ObjectType, ECollisionEnabled CollisionEnabled,
	const FVector& Scale, bool bGenerateOverlapEvents, bool bSimulatePhysics, uint16 SelfCollisionGroup)
{
	if (!Scene) return false;

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();
	physx::PxMaterial* DefaultMaterial = FPhysXSDK::Get().GetDefaultMaterial();
	if (!Physics || !DefaultMaterial) return false;

	OutInstance.OwnerComponent = OwnerComp;

	const physx::PxTransform Pose = ToPxTransform(WorldLocation, WorldRotation);

	physx::PxRigidActor* Body = nullptr;

	if (bSimulatePhysics)
	{
		Body = Physics->createRigidDynamic(Pose);
		OutInstance.Mode = EBodyInstanceMode::Dynamic;
	}
	else
	{
		Body = Physics->createRigidStatic(Pose);
		OutInstance.Mode = EBodyInstanceMode::Static;
	}

	if (!Body)
	{
		OutInstance.Body = nullptr;
		OutInstance.OwnerComponent = nullptr;
		OutInstance.Mode = EBodyInstanceMode::Static;
		return false;
	}

	const bool bTrigger = CollisionEnabled == ECollisionEnabled::QueryOnly;

	FCollisionResponseContainer Responses;
	if (OwnerComp)
	{
		Responses = OwnerComp->GetCollisionResponseContainer();
	}
	const physx::PxFilterData FilterData = MakeFilterData(ObjectType, Responses,
		CollisionEnabled, bGenerateOverlapEvents, SelfCollisionGroup);

	TArray<physx::PxShape*> Shapes;
	FPhysicsShapeFactory::CreateShapesFromBodySetup(*Physics, *DefaultMaterial, BodySetup,
		Scale, OwnerComp, bTrigger, Shapes, &FilterData);

	if (Shapes.empty())
	{
		Body->release();
		OutInstance.Body = nullptr;
		OutInstance.OwnerComponent = nullptr;
		OutInstance.Mode = EBodyInstanceMode::Static;
		return false;
	}

	for (physx::PxShape* Shape : Shapes)
	{
		if (!Shape) continue;

		Body->attachShape(*Shape);
		Shape->release();
	}

	OutInstance.Body = Body;
	Body->userData = &OutInstance;

	Scene->addActor(*Body);
	Bodies.push_back(&OutInstance);

	return true;
}

void FPhysicsScene::DestroyBody(FBodyInstance& Instance)
{
	Bodies.erase(std::remove(Bodies.begin(), Bodies.end(), &Instance), Bodies.end());

	if (Instance.Body)
	{
		if (Scene)
		{
			Scene->removeActor(*Instance.Body);
		}

		Instance.Body->userData = nullptr;
		Instance.Body->release();
		Instance.Body = nullptr;
	}

	Instance.OwnerComponent = nullptr;
	Instance.Mode = EBodyInstanceMode::Static;
}

FConstraintInstance* FPhysicsScene::CreateFixedConstraint(FBodyInstance* BodyA, FBodyInstance* BodyB,
	const FTransform& LocalFrameA, const FTransform& LocalFrameB)
{
	if (!BodyA || !BodyB || !BodyA->Body || !BodyB->Body) return nullptr;

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();

	physx::PxRigidActor* ActorA = BodyA->Body;
	physx::PxRigidActor* ActorB = BodyB->Body;

	physx::PxFixedJoint* Joint = physx::PxFixedJointCreate(*Physics, ActorA, ToPxTransform(LocalFrameA.Location, LocalFrameA.Rotation),
		ActorB, ToPxTransform(LocalFrameB.Location, LocalFrameB.Rotation));

	if (!Joint) return nullptr;

	FConstraintInstance* Instance = new FConstraintInstance();
	Instance->BodyA = BodyA;
	Instance->BodyB = BodyB;
	Instance->LocalFrameA = LocalFrameA;
	Instance->LocalFrameB = LocalFrameB;
	Instance->Joint = Joint;

	Constraints.push_back(Instance);

	return Instance;
}

FConstraintInstance* FPhysicsScene::CreateD6Constraint(FBodyInstance* BodyA, FBodyInstance* BodyB,
	const FTransform& LocalFrameA, const FTransform& LocalFrameB, EAngularConstraintMode AngularMode,
	float Swing1LimitDeg, float Swing2LimitDeg, float TwistLimitDeg)
{
	if (!BodyA || !BodyB || !BodyA->Body || !BodyB->Body) return nullptr;

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();

	physx::PxRigidActor* ActorA = BodyA->Body;
	physx::PxRigidActor* ActorB = BodyB->Body;

	physx::PxD6Joint* Joint = physx::PxD6JointCreate(*Physics,
		ActorA, ToPxTransform(LocalFrameA.Location, LocalFrameA.Rotation),
		ActorB, ToPxTransform(LocalFrameB.Location, LocalFrameB.Rotation));

	if (!Joint) return nullptr;

	// 선형 3축 고정: 두 뼈가 관절 한 점에 붙어 떨어지지 않게.
	Joint->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
	Joint->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
	Joint->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);

	// 각축 모드 → PxD6Motion
	const physx::PxD6Motion::Enum Motion =
		(AngularMode == EAngularConstraintMode::Free)   ? physx::PxD6Motion::eFREE   :
		(AngularMode == EAngularConstraintMode::Locked) ? physx::PxD6Motion::eLOCKED :
														  physx::PxD6Motion::eLIMITED;

	Joint->setMotion(physx::PxD6Axis::eTWIST,  Motion);
	Joint->setMotion(physx::PxD6Axis::eSWING1, Motion);
	Joint->setMotion(physx::PxD6Axis::eSWING2, Motion);

	if (AngularMode == EAngularConstraintMode::Limited)
	{
		const float DegToRad = 3.14159265f / 180.0f;

		// Swing: 콘 형태 (Swing1=local Y, Swing2=local Z 반각)
		Joint->setSwingLimit(physx::PxJointLimitCone(
			std::max(Swing1LimitDeg, 1.0f) * DegToRad,
			std::max(Swing2LimitDeg, 1.0f) * DegToRad));

		// Twist: 대칭 범위 [-t, +t]
		const float TwistRad = std::max(TwistLimitDeg, 1.0f) * DegToRad;
		Joint->setTwistLimit(physx::PxJointAngularLimitPair(-TwistRad, TwistRad));
	}

	FConstraintInstance* Instance = new FConstraintInstance();
	Instance->BodyA = BodyA;
	Instance->BodyB = BodyB;
	Instance->LocalFrameA = LocalFrameA;
	Instance->LocalFrameB = LocalFrameB;
	Instance->Joint = Joint; // PxD6Joint* -> PxJoint*

	Constraints.push_back(Instance);
	return Instance;
}

void FPhysicsScene::DestroyConstraint(FConstraintInstance* Instance)
{
	if (!Instance) return;

	Constraints.erase(std::remove(Constraints.begin(), Constraints.end(), Instance), Constraints.end());

	Instance->Release();
	delete Instance;
}

void FPhysicsScene::GatherClothCollision(const FClothCollisionGatherParams& Params, FClothCollisionData& OutData) const
{
	OutData.Reset();

	for (FBodyInstance* BodyInstance : Bodies)
	{
		if (!BodyInstance || !BodyInstance->Body) continue;

		UPrimitiveComponent* OwnerComp = BodyInstance->OwnerComponent;
		if (!OwnerComp || OwnerComp == Params.IgnoreComponent) continue;

		if (OwnerComp->GetOwner() == Params.IgnoreActor) continue;

		if (!OwnerComp->IsCollisionEnabled()) continue;

		if (OwnerComp->GetCollisionResponseToChannel(Params.ClothChannel) != ECollisionResponse::Block) continue;

		const FBoundingBox ClothQueryBounds = ExpandBounds(Params.WorldBounds, Params.BoundsPadding);
		const FBoundingBox BodyBounds = OwnerComp->GetWorldBoundingBox();

		if (ClothQueryBounds.IsValid() && BodyBounds.IsValid() && !ClothQueryBounds.IsIntersected(BodyBounds)) continue;

		physx::PxRigidActor* Actor = BodyInstance->Body;

		const uint32 ShapeCount = Actor->getNbShapes();
		TArray<physx::PxShape*> Shapes;
		Shapes.resize(ShapeCount);
		Actor->getShapes(Shapes.data(), ShapeCount);

		for (physx::PxShape* Shape : Shapes)
		{
			if (!Shape) continue;

			const physx::PxTransform ShapeWorldPose = Actor->getGlobalPose() * Shape->getLocalPose();

			physx::PxGeometryHolder Geometry = Shape->getGeometry();

			switch (Geometry.getType())
			{
			case physx::PxGeometryType::eSPHERE:
				AppendSphereClothCollision(Geometry.sphere(), ShapeWorldPose, Params, OutData);
				break;
			
			case physx::PxGeometryType::eCAPSULE:
				AppendCapsuleClothCollision(Geometry.capsule(), ShapeWorldPose, Params, OutData);
				break;

			case physx::PxGeometryType::eBOX:
				AppendBoxClothCollision(Geometry.box(), ShapeWorldPose, Params, OutData);
				break;

			default:
				break;
			}
		}
	}
}

bool FPhysicsScene::Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	if (!Scene || MaxDist <= 0.0f) return false;

	FVector RayDir = Dir;
	if (RayDir.IsNearlyZero()) return false;

	RayDir.Normalize();

	physx::PxRaycastBuffer HitBuffer;

	physx::PxQueryFilterData QueryFilterData;
	QueryFilterData.flags = physx::PxQueryFlag::eSTATIC |
		physx::PxQueryFlag::eDYNAMIC |
		physx::PxQueryFlag::ePREFILTER;

	FPhysicsRaycastFilterCallback FilterCallback(TraceChannel, IgnoreActor);

	const bool bHit = Scene->raycast(ToPxVec3(Start), ToPxVec3(RayDir), MaxDist, HitBuffer,
		physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX,
		QueryFilterData, &FilterCallback);

	if (!bHit || !HitBuffer.hasBlock)
	{
		OutHit = FHitResult{};
		return false;
	}

	const physx::PxRaycastHit& PxHit = HitBuffer.block;
	UPrimitiveComponent* HitComponent = GetComponentFromQueryShape(PxHit.shape);

	OutHit = FHitResult{};
	OutHit.bHit = true;
	OutHit.HitComponent = HitComponent;
	OutHit.HitActor = HitComponent ? HitComponent->GetOwner() : nullptr;
	OutHit.Distance = PxHit.distance;
	OutHit.WorldHitLocation = FromPxVec3(PxHit.position);
	OutHit.WorldNormal = FromPxVec3(PxHit.normal);
	OutHit.ImpactNormal = OutHit.WorldNormal;
	OutHit.FaceIndex = static_cast<int>(PxHit.faceIndex);

	return true;
}

bool FPhysicsScene::SweepSphere(const FVector& Start, const FVector& Dir, float MaxDist, float Radius, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	if (!Scene || MaxDist <= 0.0f || Radius <= 0.0f)
	{
		OutHit = FHitResult{};
		return false;
	}

	FVector SweepDir = Dir;
	if (SweepDir.IsNearlyZero())
	{
		OutHit = FHitResult{};
		return false;
	}

	SweepDir.Normalize();

	physx::PxSweepBuffer HitBuffer;

	physx::PxQueryFilterData QueryFilterData;
	QueryFilterData.flags = physx::PxQueryFlag::eSTATIC |
		physx::PxQueryFlag::eDYNAMIC |
		physx::PxQueryFlag::ePREFILTER;

	FPhysicsRaycastFilterCallback FilterCallback(TraceChannel, IgnoreActor);

	const physx::PxSphereGeometry SphereGeometry(Radius);
	const physx::PxTransform StartPose(ToPxVec3(Start));

	const bool bHit = Scene->sweep(SphereGeometry, StartPose, ToPxVec3(SweepDir), MaxDist, HitBuffer,
		physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX,
		QueryFilterData, &FilterCallback);

	if (!bHit || !HitBuffer.hasBlock)
	{
		OutHit = FHitResult{};
		return false;
	}

	const physx::PxSweepHit& PxHit = HitBuffer.block;
	UPrimitiveComponent* HitComponent = GetComponentFromQueryShape(PxHit.shape);

	OutHit = FHitResult{};
	OutHit.bHit = true;
	OutHit.HitComponent = HitComponent;
	OutHit.HitActor = HitComponent ? HitComponent->GetOwner() : nullptr;
	OutHit.Distance = PxHit.distance;
	OutHit.WorldHitLocation = FromPxVec3(PxHit.position);
	OutHit.WorldNormal = FromPxVec3(PxHit.normal);
	OutHit.ImpactNormal = OutHit.WorldNormal;
	OutHit.FaceIndex = static_cast<int>(PxHit.faceIndex);

	return true;
}

bool FPhysicsScene::OverlapSphere(const FVector& Center, float Radius, TArray<FOverlapResult>& OutOverlaps,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	OutOverlaps.clear();

	if (!Scene || Radius <= 0.0f) return false;

	constexpr physx::PxU32 MaxHits = 64;
	physx::PxOverlapHit Hits[MaxHits];
	physx::PxOverlapBuffer HitBuffer(Hits, MaxHits);

	physx::PxQueryFilterData QueryFilterData;
	QueryFilterData.flags = physx::PxQueryFlag::eSTATIC |
		physx::PxQueryFlag::eDYNAMIC |
		physx::PxQueryFlag::ePREFILTER;

	FPhysicsOverlapFilterCallback FilterCallback(TraceChannel, IgnoreActor);

	const bool bHit = Scene->overlap(physx::PxSphereGeometry(Radius),
		physx::PxTransform(ToPxVec3(Center)), HitBuffer,
		QueryFilterData, &FilterCallback);

	if (!bHit) return false;

	for (physx::PxU32 Index = 0; Index < HitBuffer.nbTouches; ++Index)
	{
		AppendOverlapHit(HitBuffer.touches[Index], OutOverlaps);
	}

	if (HitBuffer.hasBlock)
	{
		AppendOverlapHit(HitBuffer.block, OutOverlaps);
	}

	return !OutOverlaps.empty();
}

bool FPhysicsScene::OverlapBox(const FVector& Center, const FVector& HalfExtent, TArray<FOverlapResult>& OutOverlaps,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor)
{
	OutOverlaps.clear();

	if (!Scene || HalfExtent.X <= 0.0f || HalfExtent.Y <= 0.0f || HalfExtent.Z <= 0.0f) return false;

	constexpr physx::PxU32 MaxHits = 64;
	physx::PxOverlapHit Hits[MaxHits];
	physx::PxOverlapBuffer HitBuffer(Hits, MaxHits);

	physx::PxQueryFilterData QueryFilterData;
	QueryFilterData.flags = physx::PxQueryFlag::eSTATIC |
		physx::PxQueryFlag::eDYNAMIC |
		physx::PxQueryFlag::ePREFILTER;

	FPhysicsOverlapFilterCallback FilterCallback(TraceChannel, IgnoreActor);

	const bool bHit = Scene->overlap(physx::PxBoxGeometry(ToPxVec3(HalfExtent)),
		physx::PxTransform(ToPxVec3(Center)), HitBuffer,
		QueryFilterData, &FilterCallback);

	if (!bHit) return false;

	for (physx::PxU32 Index = 0; Index < HitBuffer.nbTouches; ++Index)
	{
		AppendOverlapHit(HitBuffer.touches[Index], OutOverlaps);
	}

	if (HitBuffer.hasBlock)
	{
		AppendOverlapHit(HitBuffer.block, OutOverlaps);
	}

	return !OutOverlaps.empty();
}

void FPhysicsScene::CollectDebugRender(FScene& RenderScene) const
{
	if (!Scene) return;

	const physx::PxRenderBuffer& Buffer = Scene->getRenderBuffer();

	const FColor Color = FColor(0, 255, 255);
	for (physx::PxU32 Index = 0; Index < Buffer.getNbLines(); ++Index)
	{
		const physx::PxDebugLine& Line = Buffer.getLines()[Index];
		RenderScene.AddDebugLine(FromPxVec3(Line.pos0), FromPxVec3(Line.pos1), Color);
	}

	if (VehicleManager)
	{
		VehicleManager->CollectDebugRender(RenderScene);
	}
}
