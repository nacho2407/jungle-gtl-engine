#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Vector.h"

class UWorld;
class AActor;

class FWorldCollisionQueries
{
public:
	static bool Raycast(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
		ECollisionChannel TraceChannel, const AActor* IgnoreActor = nullptr);

	static bool RaycastByObjectTypes(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit, 
		uint32 ObjectTypeMask, const AActor* IgnoreActor = nullptr);

	static bool SphereSweepShapeComponents(const UWorld& World, const FVector& Start, const FVector& Dir, float MaxDist, float Radius,
		FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor = nullptr);
};
