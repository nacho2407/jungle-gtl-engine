#pragma once

#include "Physics/PhysXInclude.h"

class UPrimitiveComponent;

class FPhysicsShapeFactory
{
public:
	static physx::PxShape* CreateShapeForComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
		UPrimitiveComponent* Component, bool bTrigger);
};
