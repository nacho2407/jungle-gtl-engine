#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Matrix.h"
#include "Physics/PhysXInclude.h"

class AActor;
class UPrimitiveComponent;

struct FClothCollisionData
{
	TArray<physx::PxVec4> Spheres;
	TArray<uint32> Capsules;
	TArray<physx::PxVec4> Planes;
	TArray<uint32> ConvexMasks;

	void Reset()
	{
		Spheres.clear();
		Capsules.clear();
		Planes.clear();
		ConvexMasks.clear();
	}
};

struct FClothCollisionGatherParams
{
	FBoundingBox WorldBounds;
	FMatrix WorldToCloth;

	ECollisionChannel ClothChannel = ECollisionChannel::WorldDynamic;
	float BoundsPadding = 1.0f;

	const AActor* IgnoreActor = nullptr;
	const UPrimitiveComponent* IgnoreComponent = nullptr;
};
