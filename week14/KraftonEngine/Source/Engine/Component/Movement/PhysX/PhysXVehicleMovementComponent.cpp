#include "PhysXVehicleMovementComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Physics/PhysicsScene.h"
#include "Physics/PhysXConversions.h"
#include "Physics/PhysXSDK.h"
#include "Physics/PhysXVehicleManager.h"

HIDE_FROM_COMPONENT_LIST(UPhysXVehicleMovementComponent)

void UPhysXVehicleMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return;
	}

	FPhysicsScene* PhysicsScene = World->GetPhysicsScene();
	if (!PhysicsScene)
	{
		return;
	}

	FPhysXVehicleManager* VehicleManager = PhysicsScene->GetVehicleManager();
	physx::PxPhysics* Physics = FPhysXSDK::Get().GetPhysics();
	physx::PxScene* Scene = PhysicsScene->GetPxScene();
	physx::PxMaterial* Material = FPhysXSDK::Get().GetDefaultMaterial();

	if (!VehicleManager || !Physics || !Scene || !Material)
	{
		return;
	}

	const physx::PxTransform StartPose = ToPxTransform(
		OwnerActor->GetActorLocation(),
		OwnerActor->GetActorRotation().ToQuaternion());

	if (!CreateVehicleInstance(Physics, Scene, Material, StartPose))
	{
		DestroyVehicleInstance();
		return;
	}

	physx::PxVehicleWheels* Vehicle = GetPxVehicle();
	if (!Vehicle)
	{
		DestroyVehicleInstance();
		return;
	}

	VehicleManager->RegisterVehicle(Vehicle);
	RegisteredVehicleManager = VehicleManager;
	bRegisteredWithVehicleManager = true;
}

void UPhysXVehicleMovementComponent::EndPlay()
{
	if (bRegisteredWithVehicleManager && RegisteredVehicleManager)
	{
		if (physx::PxVehicleWheels* Vehicle = GetPxVehicle())
		{
			RegisteredVehicleManager->UnregisterVehicle(Vehicle);
		}
	}

	bRegisteredWithVehicleManager = false;
	RegisteredVehicleManager = nullptr;

	DestroyVehicleInstance();

	UMovementComponent::EndPlay();
}

void UPhysXVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* OwnerActor = GetOwner();
	physx::PxRigidDynamic* VehicleActor = GetVehicleActor();
	if (!OwnerActor || !VehicleActor)
	{
		return;
	}

	const physx::PxTransform Pose = VehicleActor->getGlobalPose();
	OwnerActor->SetActorLocation(FromPxVec3(Pose.p));
	OwnerActor->SetActorRotation(FromPxQuat(Pose.q).ToRotator());
}

bool UPhysXVehicleMovementComponent::CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	(void)Physics;
	(void)Scene;
	(void)Material;
	(void)StartPose;
	return false;
}
