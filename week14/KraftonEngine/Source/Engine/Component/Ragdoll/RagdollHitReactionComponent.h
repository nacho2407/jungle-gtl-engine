#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Engine/Component/Ragdoll/RagdollHitReactionComponent.generated.h"

class AActor;
class UBoxComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FHitResult;

UCLASS()
class URagdollHitReactionComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void CreateRuntimeHitBox();
	void DestroyRuntimeHitBox(bool bRemoveFromOwner);

	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);

	bool ShouldReactToActor(AActor* OtherActor) const;
	AActor* ResolveVehicleActor();
	bool IsVehicleTouchingHitBox(AActor* VehicleActor, float DeltaTime, FVector& OutVehicleVelocity);
	FVector ComputeLaunchVelocity(AActor* OtherActor, const FVector& NormalImpulse, const FHitResult& HitResult) const;
	FVector ComputeLaunchVelocityFromVehicle(AActor* VehicleActor, const FVector& VehicleVelocity) const;
	void TriggerRagdoll(AActor* OtherActor, const FVector& LaunchVelocity, const char* SourceName);

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Vehicle Actor Name")
	FString VehicleActorName = "";

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Require Vehicle Movement Component")
	bool bRequireVehicleMovementComponent = true;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Hit Box Extent", Type=Vec3)
	FVector HitBoxExtent = FVector(0.45f, 0.45f, 0.85f);

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Hit Box Offset", Type=Vec3)
	FVector HitBoxOffset = FVector(0.0f, 0.0f, 0.85f);

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Min Launch Speed")
	float MinLaunchSpeed = 18.0f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Max Launch Speed")
	float MaxLaunchSpeed = 55.0f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Upward Launch Velocity")
	float UpwardLaunchVelocity = 8.0f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Normal Impulse To Velocity Scale")
	float NormalImpulseToVelocityScale = 0.02f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit", DisplayName="Disable Hit Box After Trigger")
	bool bDisableHitBoxAfterTrigger = true;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit|Vehicle Probe", DisplayName="Use Vehicle Proximity Check")
	bool bUseVehicleProximityCheck = true;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit|Vehicle Probe", DisplayName="Vehicle Half Extent", Type=Vec3)
	FVector VehicleHalfExtent = FVector(2.0f, 0.8f, 0.4f);

	UPROPERTY(Edit, Save, Category="Ragdoll Hit|Vehicle Probe", DisplayName="Vehicle Hit Padding")
	float VehicleHitPadding = 0.75f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit|Vehicle Probe", DisplayName="Max Vehicle Sweep Padding")
	float MaxVehicleSweepPadding = 4.0f;

	UPROPERTY(Edit, Save, Category="Ragdoll Hit|Vehicle Probe", DisplayName="Min Vehicle Speed To Trigger")
	float MinVehicleSpeedToTrigger = 1.0f;

	UBoxComponent* RuntimeHitBox = nullptr;
	USkeletalMeshComponent* CachedMesh = nullptr;
	AActor* CachedVehicleActor = nullptr;
	FDelegateHandle HitHandle;
	FVector PreviousVehicleLocation = FVector::ZeroVector;
	bool bHasPreviousVehicleLocation = false;
	bool bTriggered = false;
};
