#include "Component/Movement/VehicleMovementComponent4W.h"

#include "GameFramework/World.h"
#include "Physics/PhysXSDK.h"
#include "Physics/PhysicsScene.h"
#include "Physics/PhysXVehicleManager.h"
#include "Physics/PhysXVehicleInstance.h"
#include "Physics/PhysXConversions.h"

void UVehicleMovementComponent4W::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (FPhysicsScene* PhysicsScene = World->GetPhysicsScene())
		{
			if (FPhysXVehicleManager* VehicleManager = PhysicsScene->GetVehicleManager())
			{
				physx::PxTransform StartPose = ToPxTransform(GetOwner()->GetActorLocation(), GetOwner()->GetActorRotation().ToQuaternion());

				VehicleInstance = new FPhysXVehicleInstance();
				if (VehicleInstance->Initialize(FPhysXSDK::Get().GetPhysics(), PhysicsScene->GetPxScene(),
					FPhysXSDK::Get().GetDefaultMaterial(), StartPose))
				{
					VehicleManager->RegisterVehicle(VehicleInstance);
				}
				else
				{
					delete VehicleInstance;
					VehicleInstance = nullptr;
				}
			}
		}
	}
}

void UVehicleMovementComponent4W::EndPlay()
{
	Super::EndPlay();

	if (UWorld* World = GetWorld())
	{
		FPhysicsScene* PhysicsScene = World->GetPhysicsScene();
		if (PhysicsScene)
		{
			if (FPhysXVehicleManager* VehicleManager = PhysicsScene->GetVehicleManager())
			{
				VehicleManager->UnregisterVehicle(VehicleInstance);
			}
		}
	}

	if (VehicleInstance)
	{
		VehicleInstance->Shutdown();
		delete VehicleInstance;
		VehicleInstance = nullptr;
	}
}

void UVehicleMovementComponent4W::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (VehicleInstance)
	{
		physx::PxRigidDynamic* Actor = VehicleInstance->GetActor();
		if (!Actor) return;

		const physx::PxTransform Pose = Actor->getGlobalPose();
		GetOwner()->SetActorLocation(FromPxVec3(Pose.p));
		GetOwner()->SetActorRotation(FromPxQuat(Pose.q).ToRotator());
	}
}

void UVehicleMovementComponent4W::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UVehicleMovementComponent4W::SetDriveInput(float Throttle, float Brake, float Steer)
{
	if (VehicleInstance)
	{
		VehicleInstance->SetDriveInput(Throttle, Brake, Steer);
	}
}
