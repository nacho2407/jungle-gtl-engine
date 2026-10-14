#pragma once

#include "Physics/PhysXInclude.h"
#include "Core/Types/CollisionTypes.h"

class UPrimitiveComponent;

namespace EPhysicsFilterFlags
{
	constexpr physx::PxU32 QueryOnly = 1 << 0;
	constexpr physx::PxU32 PhysicsOnly = 1 << 1;
	constexpr physx::PxU32 QueryAndPhysics = 1 << 2;
	constexpr physx::PxU32 GenerateOverlapEvents = 1 << 3;
	constexpr physx::PxU32 DisableSelfCollision = 1 << 4;

	constexpr physx::PxU32 SelfCollisionGroupShift = 16;
	constexpr physx::PxU32 SelfCollisionGroupMask = 0xffff0000u;
}

physx::PxFilterData MakeFilterData(const UPrimitiveComponent& Component);
physx::PxFilterData MakeFilterData(ECollisionChannel ObjectType, const FCollisionResponseContainer& Responses,
	ECollisionEnabled CollisionEnabled, bool bGenerateOverlapEvents, uint16 SelfCollisionGroup = 0);
