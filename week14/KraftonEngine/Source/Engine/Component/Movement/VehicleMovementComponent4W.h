#pragma once

#include "Component/Movement/MovementComponent.h"

#include "Source/Engine/Component/Movement/VehicleMovementComponent4W.generated.h"

class FPhysXVehicleInstance;

UCLASS()
class UVehicleMovementComponent4W : public UMovementComponent
{
public:
	GENERATED_BODY()
	UVehicleMovementComponent4W() = default;
	~UVehicleMovementComponent4W() override = default;

	void BeginPlay() override;
	void EndPlay() override;

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void Serialize(FArchive& Ar) override;

	UFUNCTION(Lua)
	void SetDriveInput(float Throttle, float Brake, float Steer);

private:
	FPhysXVehicleInstance* VehicleInstance = nullptr;
};
