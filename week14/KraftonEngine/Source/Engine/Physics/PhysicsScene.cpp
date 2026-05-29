#include "Physics/PhysicsScene.h"
#include "Physics/PhysXSDK.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Physics/PhysicsShape.h"
#include "Physics/PhysXConversions.h"
#include "Physics/PhysicsEventCallback.h"
#include "Physics/PhysicsFilterShader.h"

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

	EventCallback = new FPhysicsEventCallback();
	SceneDesc.simulationEventCallback = EventCallback;

	Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = PhysicsFilterShader;

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

	Scene->addActor(*Body);
	Bodies.push_back(&OutInstance);

	return true;
}

bool FPhysicsScene::CreateBodyFromSetup(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance, const UBodySetup& BodySetup,
	const FVector& WorldLocation, const FQuat& WorldRotation, ECollisionChannel ObjectType, ECollisionEnabled CollisionEnabled,
	const FVector& Scale, bool bGenerateOverlapEvents, bool bSimulatePhysics)
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

	TArray<physx::PxShape*> Shapes;
	FPhysicsShapeFactory::CreateShapesFromBodySetup(*Physics, *DefaultMaterial, BodySetup, Scale, OwnerComp, bTrigger, Shapes);

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

void FPhysicsScene::DestroyConstraint(FConstraintInstance* Instance)
{
	if (!Instance) return;

	Constraints.erase(std::remove(Constraints.begin(), Constraints.end(), Instance), Constraints.end());

	Instance->Release();
	delete Instance;
}
