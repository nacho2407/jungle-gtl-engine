#include "Physics/PhysicsScene.h"
#include "Physics/PhysXSDK.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Physics/PhysicsShape.h"
#include "Physics/PhysXConversions.h"

#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"

#include <algorithm>

void FPhysicsScene::Initialize()
{
	FPhysXSDK::Get().Initialize();

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();

	physx::PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = physx::PxVec3(0.0f, 0.0f, -980.0f);

	Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

	Scene = Physics->createScene(SceneDesc);
}

void FPhysicsScene::Shutdown()
{
	while (!Constraints.empty())
	{
		DestroyConstraint(Constraints.back());
	}

	while (!Bodies.empty())
	{
		DestroyBody(Bodies.back());
	}

	if (Scene)
	{
		Scene->release();
		Scene = nullptr;
	}
	if (Dispatcher)
	{
		Dispatcher->release();
		Dispatcher = nullptr;
	}
}

void FPhysicsScene::Simulate(float DeltaTime)
{
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

FBodyInstance* FPhysicsScene::CreateBody(UPrimitiveComponent* OwnerComp)
{
	if (!Scene || !OwnerComp) return nullptr;

	if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(OwnerComp))
	{
		UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
		UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;

		if (!BodySetup) return nullptr;

		return CreateBodyFromSetup(OwnerComp, *BodySetup, OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion(),
			OwnerComp->GetCollisionObjectType(), OwnerComp->GetCollisionEnabled(), OwnerComp->GetWorldScale(), OwnerComp->GetGenerateOverlapEvents());
	}

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();
	physx::PxMaterial* DefaultMaterial = FPhysXSDK::Get().GetDefaultMaterial();

	FBodyInstance* Instance = new FBodyInstance();
	Instance->OwnerComponent = OwnerComp;

	physx::PxRigidActor* Body = nullptr;

	if (OwnerComp->GetCollisionObjectType() == ECollisionChannel::WorldStatic)
	{
		Body = Physics->createRigidStatic(ToPxTransform(OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion()));
		Instance->Mode = EBodyInstanceMode::Static;
	}
	else
	{
		Body = Physics->createRigidDynamic(ToPxTransform(OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion()));
		Instance->Mode = EBodyInstanceMode::Dynamic;
	}

	if (!Body)
	{
		delete Instance;
		return nullptr;
	}

	const bool bTrigger = OwnerComp->GetGenerateOverlapEvents() || OwnerComp->GetCollisionEnabled() == ECollisionEnabled::QueryOnly;

	TArray<physx::PxShape*> Shapes;
	FPhysicsShapeFactory::CreateShapesForComponent(*Physics, *DefaultMaterial, OwnerComp, bTrigger, Shapes);

	if (Shapes.empty())
	{
		Body->release();
		delete Instance;
		return nullptr;
	}

	for (physx::PxShape* Shape : Shapes)
	{
		if (!Shape) continue;

		Body->attachShape(*Shape);
		Shape->release();
	}

	Instance->Body = Body;
	Body->userData = Instance;

	Scene->addActor(*Body);
	Bodies.push_back(Instance);

	return Instance;
}

FBodyInstance* FPhysicsScene::CreateBodyFromSetup(UPrimitiveComponent* OwnerComp, const UBodySetup& BodySetup,
	const FVector& WorldLocation, const FQuat& WorldRotation, ECollisionChannel ObjectType, ECollisionEnabled CollisionEnabled,
	const FVector& Scale, bool bGenerateOverlapEvents)
{
	if (!Scene) return nullptr;

	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();
	physx::PxMaterial* DefaultMaterial = FPhysXSDK::Get().GetDefaultMaterial();
	if (!Physics || !DefaultMaterial) return nullptr;

	FBodyInstance* Instance = new FBodyInstance();
	Instance->OwnerComponent = OwnerComp;

	const physx::PxTransform Pose = ToPxTransform(WorldLocation, WorldRotation);

	physx::PxRigidActor* Body = nullptr;

	if (ObjectType == ECollisionChannel::WorldStatic)
	{
		Body = Physics->createRigidStatic(Pose);
		Instance->Mode = EBodyInstanceMode::Static;
	}
	else
	{
		Body = Physics->createRigidDynamic(Pose);
		Instance->Mode = EBodyInstanceMode::Dynamic;
	}

	if (!Body)
	{
		delete Instance;
		return nullptr;
	}

	const bool bTrigger = bGenerateOverlapEvents || CollisionEnabled == ECollisionEnabled::QueryOnly;

	TArray<physx::PxShape*> Shapes;
	FPhysicsShapeFactory::CreateShapesFromBodySetup(*Physics, *DefaultMaterial, BodySetup, Scale, OwnerComp, bTrigger, Shapes);

	if (Shapes.empty())
	{
		Body->release();
		delete Instance;
		return nullptr;
	}

	for (physx::PxShape* Shape : Shapes)
	{
		if (!Shape) continue;

		Body->attachShape(*Shape);
		Shape->release();
	}

	Instance->Body = Body;
	Body->userData = Instance;

	Scene->addActor(*Body);
	Bodies.push_back(Instance);

	return Instance;
}

void FPhysicsScene::DestroyBody(FBodyInstance* Instance)
{
	if (!Instance) return;

	Bodies.erase(std::remove(Bodies.begin(), Bodies.end(), Instance), Bodies.end());

	if (Instance->Body)
	{
		if (Scene)
		{
			Scene->removeActor(*Instance->Body);
		}

		Instance->Body->userData = nullptr;
		Instance->Body->release();
		Instance->Body = nullptr;
	}
	delete Instance;
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

void FPhysicsScene::DestroyConstraint(FConstraintInstance* Instance)
{
	if (!Instance) return;

	Constraints.erase(std::remove(Constraints.begin(), Constraints.end(), Instance), Constraints.end());

	Instance->Release();
	delete Instance;
}
