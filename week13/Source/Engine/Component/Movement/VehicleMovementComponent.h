#pragma once

#include "Component/Movement/MovementComponent.h"
#include "Object/Ptr/ObjectPtr.h"

#include <PxPhysicsAPI.h>

#include "Source/Engine/Component/Movement/VehicleMovementComponent.generated.h"

class UWheelMeshComponent;

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
	void SetWheelMeshComponents(const TArray<UWheelMeshComponent*>& InWheelMeshComponents);

private:
	bool InitializeVehicle();
	void ReleaseVehicle();
	void ApplyKeyboardInput(float DeltaTime);
	void UpdateBoost(float DeltaTime);
	void SetRuntimePeakTorque(float PeakTorque);
	void UpdateWheelVisuals();
	void DrawDebugVehicle() const;

	UPROPERTY(Transient, Category="Vehicle")
	TArray<TObjectPtr<USceneComponent>> WheelSceneComponents;
	UPROPERTY(Transient, Category="Vehicle")
	TArray<TObjectPtr<UWheelMeshComponent>> WheelMeshComponents;

	UPROPERTY(Edit, Save, Category="Vehicle|Debug", DisplayName="Visible")
	bool bDebugVisualizationVisible = false;

	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Radius", Min=0.01f, Max=10.0f, Speed=0.01f)
	float WheelRadius = 0.38f;
	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Width", Min=0.01f, Max=10.0f, Speed=0.01f)
	float WheelWidth = 0.3f;
	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Mass", Min=0.01f, Max=1000.0f, Speed=0.1f)
	float WheelMass = 20.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Moment Of Inertia Scale", Min=0.01f, Max=10.0f, Speed=0.01f)
	float WheelMomentOfInertiaScale = 0.5f;
	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Front Max Steer Degrees", Min=0.0f, Max=90.0f, Speed=0.1f)
	float FrontWheelMaxSteerDegrees = 45.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Wheel", DisplayName="Rear Hand Brake Torque", Min=0.0f, Max=100000.0f, Speed=10.0f)
	float RearWheelMaxHandBrakeTorque = 4000.0f;

	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Max Compression", Min=0.0f, Max=10.0f, Speed=0.01f)
	float SuspensionMaxCompression = 0.4f;
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Max Droop", Min=0.0f, Max=10.0f, Speed=0.01f)
	float SuspensionMaxDroop = 0.1f;
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Spring Strength", Min=0.0f, Max=1000000.0f, Speed=100.0f)
	float SuspensionSpringStrength = 50000.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Spring Damper Rate", Min=0.0f, Max=100000.0f, Speed=10.0f)
	float SuspensionSpringDamperRate = 2500.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Travel Direction", Type=Vec3, Min=-1.0f, Max=1.0f, Speed=0.01f)
	FVector SuspensionTravelDirection = FVector(0.0f, 0.0f, -1.0f);
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Suspension Force Application Z", Min=-10.0f, Max=10.0f, Speed=0.01f)
	float SuspensionForceApplicationOffsetZ = -0.3f;
	UPROPERTY(Edit, Save, Category="Vehicle|Suspension", DisplayName="Tire Force Application Z", Min=-10.0f, Max=10.0f, Speed=0.01f)
	float TireForceApplicationOffsetZ = -0.3f;

	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Peak Torque", Min=0.0f, Max=100000.0f, Speed=10.0f)
	float EnginePeakTorque = 500.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Max Omega", Min=0.0f, Max=10000.0f, Speed=10.0f)
	float EngineMaxOmega = 600.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 0 Omega", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve0Omega = 0.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 0 Torque", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve0Torque = 0.2f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 1 Omega", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve1Omega = 0.2f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 1 Torque", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve1Torque = 0.6f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 2 Omega", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve2Omega = 0.5f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 2 Torque", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve2Torque = 1.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 3 Omega", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve3Omega = 1.0f;
	UPROPERTY(Edit, Save, Category="Vehicle|Engine", DisplayName="Torque Curve 3 Torque", Min=0.0f, Max=1.0f, Speed=0.01f)
	float EngineTorqueCurve3Torque = 0.2f;

	UPROPERTY(Edit, Save, Category="Vehicle|Boost", DisplayName="Duration", Min=0.0f, Max=60.0f, Speed=0.1f)
	float BoostDuration = 1.5f;
	UPROPERTY(Edit, Save, Category="Vehicle|Boost", DisplayName="Torque Multiplier", Min=0.0f, Max=100.0f, Speed=0.1f)
	float BoostTorqueMultiplier = 2.0f;

	UPROPERTY(Edit, Save, Category="Vehicle|Transmission", DisplayName="Gear Ratio Count", Min=3.0f, Max=7.0f, Speed=1.0f)
	int32 GearRatioCount = 7;
	UPROPERTY(Edit, Save, Category="Vehicle|Transmission", DisplayName="Use Auto Gears")
	bool bUseAutoGears = true;

	UPROPERTY(Edit, Save, Category="Vehicle|Steering", DisplayName="Ackermann Accuracy", Min=0.0f, Max=1.0f, Speed=0.01f)
	float AckermannAccuracy = 1.0f;

	UPROPERTY(Edit, Save, Category="Vehicle|Tire", DisplayName="Tire Type", Min=0.0f, Max=100.0f, Speed=1.0f)
	int32 TireType = 0;
	UPROPERTY(Edit, Save, Category="Vehicle|Tire", DisplayName="Surface Friction", Min=0.0f, Max=10.0f, Speed=0.01f)
	float SurfaceFriction = 1.0f;

	physx::PxVehicleDrive4W* VehicleDrive = nullptr;
	physx::PxBatchQuery* SuspensionBatchQuery = nullptr;
	physx::PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	physx::PxRaycastQueryResult SuspensionRaycastResults[WheelCount];
	physx::PxRaycastHit SuspensionRaycastHits[WheelCount];
	physx::PxWheelQueryResult WheelQueryResults[WheelCount];
	physx::PxVehicleWheelQueryResult VehicleWheelQueryResult{};

	bool bBoostActive = false;
	float BoostRemainingTime = 0.0f;
};
