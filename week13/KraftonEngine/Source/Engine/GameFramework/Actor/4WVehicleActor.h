#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/4WVehicleActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UVehicleMovementComponent;

UCLASS()
class A4WVehicleActor : public AActor
{
public:
	GENERATED_BODY()

	A4WVehicleActor() = default;
	~A4WVehicleActor() override = default;

	void BeginPlay() override;
	void PostDuplicate() override;
	void InitDefaultComponents();

	UBoxComponent* GetChassisCollision() const { return ChassisCollision; }
	UStaticMeshComponent* GetChassisMesh() const { return ChassisMesh; }
	UVehicleMovementComponent* GetVehicleMovementComponent() const { return VehicleMovementComponent; }

protected:
	void OnOwnedComponentRemoved(UActorComponent* Component) override;

private:
	void RebindComponentReferences();

	UBoxComponent* ChassisCollision = nullptr;
	UStaticMeshComponent* ChassisMesh = nullptr;
	USceneComponent* WheelPivots[4] = {};
	UStaticMeshComponent* WheelMeshes[4] = {};
	UVehicleMovementComponent* VehicleMovementComponent = nullptr;
};
