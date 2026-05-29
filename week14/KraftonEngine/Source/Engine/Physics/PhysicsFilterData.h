#pragma once

#include "Physics/PhysXInclude.h"

class UPrimitiveComponent;

namespace EPhysicsFilterFlags
{
	constexpr physx::PxU32 QueryOnly = 1 << 0;
	constexpr physx::PxU32 PhysicsOnly = 1 << 1;
	constexpr physx::PxU32 QueryAndPhysics = 1 << 2;
	constexpr physx::PxU32 GenerateOverlapEvents = 1 << 3;
}

physx::PxFilterData MakeFilterData(const UPrimitiveComponent& Component);
