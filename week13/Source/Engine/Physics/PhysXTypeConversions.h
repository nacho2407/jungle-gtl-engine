#pragma once

#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Transform.h"

#include <PxPhysicsAPI.h>

namespace PhysXConvert
{
	inline physx::PxVec3 ToPxVec3(const FVector& V)
	{
		return physx::PxVec3(V.X, V.Y, V.Z);
	}

	inline FVector ToFVector(const physx::PxVec3& V)
	{
		return FVector(V.x, V.y, V.z);
	}

	inline physx::PxQuat ToPxQuat(const FQuat& Q)
	{
		FQuat Normalized = Q;
		Normalized.Normalize();

		return physx::PxQuat(
			Normalized.X,
			Normalized.Y,
			Normalized.Z,
			Normalized.W
		);
	}

	inline FQuat ToFQuat(const physx::PxQuat& Q)
	{
		FQuat Result(Q.x, Q.y, Q.z, Q.w);
		Result.Normalize();
		return Result;
	}

	inline physx::PxTransform ToPxTransform(const FTransform& T)
	{
		// PxTransform에는 Scale이 없음
		return physx::PxTransform(
			ToPxVec3(T.Location),
			ToPxQuat(T.Rotation)
		);
	}

	inline FTransform ToFTransform(const physx::PxTransform& T)
	{
		return FTransform(
			ToFVector(T.p),
			ToFQuat(T.q),
			FVector::OneVector
		);
	}
}