#pragma once
#include "Component/Movement/MovementComponent.h"
#include "Physics/PhysXInclude.h"

#include "Source/Engine/Component/Movement/PhysX/PhysXVehicleMovementComponent.generated.h"

UCLASS()
class UPhysXVehicleMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()

	UPhysXVehicleMovementComponent() = default;
	~UPhysXVehicleMovementComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	virtual void SyncFromPhysics();

protected:
	virtual bool CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene, physx::PxMaterial* Material, const physx::PxTransform& StartPose);
	virtual void DestroyVehicleInstance() {}
	virtual physx::PxVehicleWheels* GetPxVehicle() const { return nullptr; }
	virtual physx::PxRigidDynamic* GetVehicleActor() const { return nullptr; }

private:
	class FPhysXVehicleManager* RegisteredVehicleManager = nullptr;
	bool bRegisteredWithVehicleManager = false;
};
