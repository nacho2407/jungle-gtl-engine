#include "PhysicsStats.h"

#if STATS
uint32 FPhysicsStats::PhysXSceneCount = 0;
uint32 FPhysicsStats::PhysXBodyCount = 0;
uint32 FPhysicsStats::PhysXStaticBodyCount = 0;
uint32 FPhysicsStats::PhysXDynamicBodyCount = 0;
uint32 FPhysicsStats::PhysXConstraintCount = 0;
uint32 FPhysicsStats::PhysXActiveActorCount = 0;
uint32 FPhysicsStats::PhysXVehicleCount = 0;
uint32 FPhysicsStats::PhysXControllerCount = 0;

uint32 FPhysicsStats::ActiveRagdollCount = 0;
uint32 FPhysicsStats::RagdollBodyCount = 0;
uint32 FPhysicsStats::RagdollConstraintCount = 0;
uint32 FPhysicsStats::RagdollStartAttemptCount = 0;
uint32 FPhysicsStats::RagdollStartSuccessCount = 0;
uint32 FPhysicsStats::RagdollStartFailCount = 0;

uint32 FPhysicsStats::ActiveClothCount = 0;
uint32 FPhysicsStats::ClothParticleCount = 0;
uint32 FPhysicsStats::ClothTriangleCount = 0;
uint32 FPhysicsStats::ClothSubstepCount = 0;
uint32 FPhysicsStats::ClothCollisionSphereCount = 0;
uint32 FPhysicsStats::ClothCollisionCapsuleCount = 0;
uint32 FPhysicsStats::ClothCollisionPlaneCount = 0;
uint32 FPhysicsStats::ClothCollisionConvexCount = 0;

void FPhysicsStats::Reset()
{
	PhysXSceneCount = 0;
	PhysXBodyCount = 0;
	PhysXStaticBodyCount = 0;
	PhysXDynamicBodyCount = 0;
	PhysXConstraintCount = 0;
	PhysXActiveActorCount = 0;
	PhysXVehicleCount = 0;
	PhysXControllerCount = 0;

	ActiveRagdollCount = 0;
	RagdollBodyCount = 0;
	RagdollConstraintCount = 0;
	RagdollStartAttemptCount = 0;
	RagdollStartSuccessCount = 0;
	RagdollStartFailCount = 0;

	ActiveClothCount = 0;
	ClothParticleCount = 0;
	ClothTriangleCount = 0;
	ClothSubstepCount = 0;
	ClothCollisionSphereCount = 0;
	ClothCollisionCapsuleCount = 0;
	ClothCollisionPlaneCount = 0;
	ClothCollisionConvexCount = 0;
}

void FPhysicsStats::RecordPhysXScene(uint32 BodyCount, uint32 StaticBodyCount, uint32 DynamicBodyCount,
	uint32 ConstraintCount, uint32 ActiveActorCount, uint32 VehicleCount, uint32 ControllerCount)
{
	++PhysXSceneCount;
	PhysXBodyCount += BodyCount;
	PhysXStaticBodyCount += StaticBodyCount;
	PhysXDynamicBodyCount += DynamicBodyCount;
	PhysXConstraintCount += ConstraintCount;
	PhysXActiveActorCount += ActiveActorCount;
	PhysXVehicleCount += VehicleCount;
	PhysXControllerCount += ControllerCount;
}

void FPhysicsStats::RecordRagdollActive(uint32 BodyCount, uint32 ConstraintCount)
{
	++ActiveRagdollCount;
	RagdollBodyCount += BodyCount;
	RagdollConstraintCount += ConstraintCount;
}

void FPhysicsStats::RecordClothActive(uint32 ParticleCount, uint32 TriangleCount, uint32 SubstepCount)
{
	++ActiveClothCount;
	ClothParticleCount += ParticleCount;
	ClothTriangleCount += TriangleCount;
	ClothSubstepCount += SubstepCount;
}

void FPhysicsStats::RecordClothCollision(uint32 SphereCount, uint32 CapsuleCount, uint32 PlaneCount, uint32 ConvexCount)
{
	ClothCollisionSphereCount += SphereCount;
	ClothCollisionCapsuleCount += CapsuleCount;
	ClothCollisionPlaneCount += PlaneCount;
	ClothCollisionConvexCount += ConvexCount;
}

void FPhysicsStats::RecordRagdollStart(bool bSuccess)
{
	++RagdollStartAttemptCount;
	if (bSuccess)
	{
		++RagdollStartSuccessCount;
	}
	else
	{
		++RagdollStartFailCount;
	}
}
#endif
