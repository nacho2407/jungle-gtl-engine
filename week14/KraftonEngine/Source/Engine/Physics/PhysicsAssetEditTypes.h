#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Quat.h"
#include "Math/Vector.h"

enum class EPhysicsAssetPrimitiveType : uint8
{
	Box,
	Capsule,
	Sphere
};

struct FPhysicsAssetBodyShapeDesc
{
	EPhysicsAssetPrimitiveType PrimitiveType = EPhysicsAssetPrimitiveType::Capsule;
	FVector Center = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;

	// Box: half extents. Sphere: X is radius.
	// Capsule: X is radius, Z is FKSphylElem::Length.
	FVector Extents = FVector::OneVector;

	static FPhysicsAssetBodyShapeDesc MakeBox(const FVector& InCenter, const FQuat& InRotation, const FVector& InExtents)
	{
		FPhysicsAssetBodyShapeDesc Desc;
		Desc.PrimitiveType = EPhysicsAssetPrimitiveType::Box;
		Desc.Center = InCenter;
		Desc.Rotation = InRotation;
		Desc.Extents = InExtents;
		return Desc;
	}

	static FPhysicsAssetBodyShapeDesc MakeSphere(const FVector& InCenter, float InRadius)
	{
		FPhysicsAssetBodyShapeDesc Desc;
		Desc.PrimitiveType = EPhysicsAssetPrimitiveType::Sphere;
		Desc.Center = InCenter;
		Desc.Rotation = FQuat::Identity;
		Desc.Extents = FVector(InRadius, InRadius, InRadius);
		return Desc;
	}

	static FPhysicsAssetBodyShapeDesc MakeCapsule(const FVector& InCenter, const FQuat& InRotation, float InRadius, float InLength)
	{
		FPhysicsAssetBodyShapeDesc Desc;
		Desc.PrimitiveType = EPhysicsAssetPrimitiveType::Capsule;
		Desc.Center = InCenter;
		Desc.Rotation = InRotation;
		Desc.Extents = FVector(InRadius, InRadius, InLength);
		return Desc;
	}
};
