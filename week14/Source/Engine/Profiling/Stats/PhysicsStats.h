#pragma once

#include "Core/Types/CoreTypes.h"
#include "Profiling/Stats/Stats.h"

#if STATS
struct FPhysicsStats
{
	static uint32 PhysXSceneCount;
	static uint32 PhysXBodyCount;
	static uint32 PhysXStaticBodyCount;
	static uint32 PhysXDynamicBodyCount;
	static uint32 PhysXConstraintCount;
	static uint32 PhysXActiveActorCount;
	static uint32 PhysXVehicleCount;
	static uint32 PhysXControllerCount;

	static uint32 ActiveRagdollCount;
	static uint32 RagdollBodyCount;
	static uint32 RagdollConstraintCount;
	static uint32 RagdollStartAttemptCount;
	static uint32 RagdollStartSuccessCount;
	static uint32 RagdollStartFailCount;

	static uint32 ActiveClothCount;
	static uint32 ClothParticleCount;
	static uint32 ClothTriangleCount;
	static uint32 ClothSubstepCount;
	static uint32 ClothCollisionSphereCount;
	static uint32 ClothCollisionCapsuleCount;
	static uint32 ClothCollisionPlaneCount;
	static uint32 ClothCollisionConvexCount;

	static void Reset();
	static void RecordPhysXScene(uint32 BodyCount, uint32 StaticBodyCount, uint32 DynamicBodyCount,
		uint32 ConstraintCount, uint32 ActiveActorCount, uint32 VehicleCount, uint32 ControllerCount);
	static void RecordRagdollActive(uint32 BodyCount, uint32 ConstraintCount);
	static void RecordClothActive(uint32 ParticleCount, uint32 TriangleCount, uint32 SubstepCount);
	static void RecordClothCollision(uint32 SphereCount, uint32 CapsuleCount, uint32 PlaneCount, uint32 ConvexCount);
	static void RecordRagdollStart(bool bSuccess);
};

#define PHYSICS_STATS_RESET() FPhysicsStats::Reset()
#define PHYSICS_STATS_RECORD_PHYSX_SCENE(BodyCount, StaticBodyCount, DynamicBodyCount, ConstraintCount, ActiveActorCount, VehicleCount, ControllerCount) \
	FPhysicsStats::RecordPhysXScene(BodyCount, StaticBodyCount, DynamicBodyCount, ConstraintCount, ActiveActorCount, VehicleCount, ControllerCount)
#define PHYSICS_STATS_RECORD_RAGDOLL_ACTIVE(BodyCount, ConstraintCount) FPhysicsStats::RecordRagdollActive(BodyCount, ConstraintCount)
#define PHYSICS_STATS_RECORD_CLOTH_ACTIVE(ParticleCount, TriangleCount, SubstepCount) FPhysicsStats::RecordClothActive(ParticleCount, TriangleCount, SubstepCount)
#define PHYSICS_STATS_RECORD_CLOTH_COLLISION(SphereCount, CapsuleCount, PlaneCount, ConvexCount) FPhysicsStats::RecordClothCollision(SphereCount, CapsuleCount, PlaneCount, ConvexCount)
#define PHYSICS_STATS_RECORD_RAGDOLL_START(bSuccess) FPhysicsStats::RecordRagdollStart(bSuccess)
#else
#define PHYSICS_STATS_RESET() ((void)0)
#define PHYSICS_STATS_RECORD_PHYSX_SCENE(BodyCount, StaticBodyCount, DynamicBodyCount, ConstraintCount, ActiveActorCount, VehicleCount, ControllerCount) ((void)0)
#define PHYSICS_STATS_RECORD_RAGDOLL_ACTIVE(BodyCount, ConstraintCount) ((void)0)
#define PHYSICS_STATS_RECORD_CLOTH_ACTIVE(ParticleCount, TriangleCount, SubstepCount) ((void)0)
#define PHYSICS_STATS_RECORD_CLOTH_COLLISION(SphereCount, CapsuleCount, PlaneCount, ConvexCount) ((void)0)
#define PHYSICS_STATS_RECORD_RAGDOLL_START(bSuccess) ((void)0)
#endif
