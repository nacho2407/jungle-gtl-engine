#pragma once

#include "Physics/PhysicsTypes.h"
#include "Physics/PhysXInclude.h"
#include "Physics/ClothCollisionTypes.h"

#include "Core/Types/CoreTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Transform.h"

class AActor;
class UPrimitiveComponent;
class FScene;
class UBodySetup;
class FPhysicsEventCallback;
class FPhysXVehicleManager;
struct FBodyInstance;
struct FConstraintInstance;
enum class EAngularConstraintMode : uint8;

class FPhysicsScene
{
public:
	void Initialize();
	void Shutdown();
	void Simulate(float DeltaTime);

	bool CreateBody(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance);
	bool CreateBodyFromSetup(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance, const UBodySetup& BodySetup,
		const FVector& WorldLocation, const FQuat& WorldRotation, ECollisionChannel ObjectType, ECollisionEnabled CollisionEnabled,
		const FVector& Scale, bool bGenerateOverlapEvents, bool bSimulatePhysics, uint16 SelfCollisionGroup = 0);
	void DestroyBody(FBodyInstance& Instance);

	FConstraintInstance* CreateFixedConstraint(FBodyInstance* BodyA, FBodyInstance* BodyB,
		const FTransform& LocalFrameA, const FTransform& LocalFrameB);
	FConstraintInstance* CreateD6Constraint(FBodyInstance* BodyA, FBodyInstance* BodyB,
		const FTransform& LocalFrameA, const FTransform& LocalFrameB, EAngularConstraintMode AngularMode,
		float Swing1LimitDeg, float Swing2LimitDeg, float TwistLimitDeg);
	void DestroyConstraint(FConstraintInstance* Instance);

	void GatherClothCollision(const FClothCollisionGatherParams& Params, FClothCollisionData& OutData) const;

	bool Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr);
	bool SweepSphere(const FVector& Start, const FVector& Dir, float MaxDist, float Radius, FHitResult& OutHit,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr);

	bool OverlapSphere(const FVector& Center, float Radius, TArray<FOverlapResult>& OutOverlaps,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr);
	bool OverlapBox(const FVector& Center, const FVector& HalfExtent, TArray<FOverlapResult>& OutOverlaps,
		ECollisionChannel TraceChannel = ECollisionChannel::WorldStatic,
		const AActor* IgnoreActor = nullptr);

	void CollectDebugRender(FScene& RenderScene) const;

	physx::PxScene* GetPxScene() const { return Scene; }

	FPhysXVehicleManager* GetVehicleManager() const { return VehicleManager; }

private:
	physx::PxScene* Scene = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;

	FPhysicsEventCallback* EventCallback = nullptr;

	FPhysXVehicleManager* VehicleManager = nullptr;

	TArray<FBodyInstance*> Bodies;
	TArray<FConstraintInstance*> Constraints;
};
