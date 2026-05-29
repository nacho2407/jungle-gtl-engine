#pragma once

#include "Physics/PhysicsTypes.h"
#include "Physics/PhysXInclude.h"

#include "Core/Types/CoreTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Transform.h"

class UPrimitiveComponent;
class UBodySetup;
struct FBodyInstance;
struct FConstraintInstance;

class FPhysicsScene
{
public:
	void Initialize();
	void Shutdown();
	void Simulate(float DeltaTime);

	bool CreateBody(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance);
	bool CreateBodyFromSetup(UPrimitiveComponent* OwnerComp, FBodyInstance& OutInstance, const UBodySetup& BodySetup,
		const FVector& WorldLocation, const FQuat& WorldRotation, ECollisionChannel ObjectType, ECollisionEnabled CollisionEnabled,
		const FVector& Scale, bool bGenerateOverlapEvents, bool bSimulatePhysics);
	void DestroyBody(FBodyInstance& Instance);

	FConstraintInstance* CreateFixedConstraint(FBodyInstance* BodyA, FBodyInstance* BodyB,
		const FTransform& LocalFrameA, const FTransform& LocalFrameB);
	void DestroyConstraint(FConstraintInstance* Instance);

	physx::PxScene* GetPxScene() const { return Scene; }

private:
	physx::PxScene* Scene = nullptr;
	physx::PxDefaultCpuDispatcher* Dispatcher = nullptr;

	TArray<FBodyInstance*> Bodies;
	TArray<FConstraintInstance*> Constraints;
};
