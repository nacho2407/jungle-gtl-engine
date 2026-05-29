#pragma once

#include "Physics/PhysXInclude.h"
#include "Core/Types/CoreTypes.h"

class UPrimitiveComponent;
class UStaticMeshComponent;

class FPhysicsShapeFactory
{
public:
	static void CreateShapesForComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
		UPrimitiveComponent* Component, bool bTrigger, TArray<physx::PxShape*>& OutShapes);

private:
	static void CreateShapesForStaticMeshComponent(physx::PxPhysics& Physics, physx::PxMaterial& Material,
		UStaticMeshComponent* Component, bool bTrigger, TArray<physx::PxShape*>& OutShapes);

	static void ApplyShapeFlags(physx::PxShape& Shape, UPrimitiveComponent* Component, bool bTrigger);
};
