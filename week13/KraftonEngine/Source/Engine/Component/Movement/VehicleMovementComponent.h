#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Object/Ptr/ObjectPtr.h"

#include <PxPhysicsAPI.h>

#include "Source/Engine/Component/Movement/VehicleMovementComponent.generated.h"

UCLASS()
class UVehicleMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()

	static constexpr int32 WheelCount = 4;

	UVehicleMovementComponent() = default;
	~UVehicleMovementComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void BeginDestroy() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void PostEditProperty(const char* PropertyName) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	void SetWheelSceneComponents(const TArray<USceneComponent*>& InWheelSceneComponents);

private:
	bool InitializeVehicle();
	void ReleaseVehicle();
	void ApplyKeyboardInput(float DeltaTime);
	void UpdateWheelVisuals();

	UPROPERTY(Transient, Category="Vehicle")
	TArray<TObjectPtr<USceneComponent>> WheelSceneComponents;

	physx::PxVehicleDrive4W* VehicleDrive = nullptr;
	physx::PxBatchQuery* SuspensionBatchQuery = nullptr;
	physx::PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	physx::PxRaycastQueryResult SuspensionRaycastResults[WheelCount];
	physx::PxRaycastHit SuspensionRaycastHits[WheelCount];
	physx::PxWheelQueryResult WheelQueryResults[WheelCount];
	physx::PxVehicleWheelQueryResult VehicleWheelQueryResult{};
};
