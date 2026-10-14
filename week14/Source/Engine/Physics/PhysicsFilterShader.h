#pragma once

#include "Physics/PhysXInclude.h"

physx::PxFilterFlags PhysicsFilterShader(
	physx::PxFilterObjectAttributes Attributes0,
	physx::PxFilterData FilterData0,
	physx::PxFilterObjectAttributes Attributes1,
	physx::PxFilterData FilterData1,
	physx::PxPairFlags& PairFlags,
	const void* ConstantBlock,
	physx::PxU32 ConstantBlockSize);
