#pragma once

#include "Physics/PhysXInclude.h"

struct FClothCollisionData;
struct FClothCollisionGatherParams;

void AppendShapeClothCollision(
	const physx::PxShape& Shape,
	const physx::PxTransform& ShapeWorldPose,
	const FClothCollisionGatherParams& Params,
	FClothCollisionData& OutData);
