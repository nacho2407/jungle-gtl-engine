#include "Physics/PhysicsScene.h"
#include "Physics/BodyInstance.h"
#include "Physics/PhysicsShape.h"
#include "Physics/PhysXConversions.h"

#include "Component/PrimitiveComponent.h"

#include <algorithm>

void FPhysicsScene::Initialize()
{
	Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, Allocator, ErrorCallback);
	Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *Foundation, physx::PxTolerancesScale());

	physx::PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = physx::PxVec3(0.0f, 0.0f, -980.0f);
	Dispatcher = physx::PxDefaultCpuDispatcherCreate(2);
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

	Scene = Physics->createScene(SceneDesc);
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.6f);
}

void FPhysicsScene::Shutdown()
{
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
	if (DefaultMaterial)
	{
		DefaultMaterial->release();
		DefaultMaterial = nullptr;
	}
	if (Physics)
	{
		Physics->release();
		Physics = nullptr;
	}
	if (Foundation)
	{
		Foundation->release();
		Foundation = nullptr;
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
	if (!Physics || !Scene || !OwnerComp) return nullptr;

	FBodyInstance* Instance = new FBodyInstance();
	Instance->OwnerComponent = OwnerComp;

	physx::PxTransform Pose = ToPxTransform(OwnerComp->GetWorldLocation(), OwnerComp->GetWorldRotation().ToQuaternion());
	physx::PxRigidActor* Body = nullptr;

	if (OwnerComp->GetCollisionObjectType() == ECollisionChannel::WorldStatic)
	{
		Body = Physics->createRigidStatic(Pose);
	}
	else
	{
		Body = Physics->createRigidDynamic(Pose);
	}

	if (!Body)
	{
		delete Instance;
		return nullptr;
	}

	const bool bTrigger = OwnerComp->GetGenerateOverlapEvents() || OwnerComp->GetCollisionEnabled() == ECollisionEnabled::QueryOnly;

	physx::PxShape* Shape = FPhysicsShapeFactory::CreateShapeForComponent(*Physics, *DefaultMaterial, OwnerComp, bTrigger);
	if (!Shape)
	{
		Body->release();
		delete Instance;
		return nullptr;
	}

	Body->attachShape(*Shape);
	Shape->release();

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
