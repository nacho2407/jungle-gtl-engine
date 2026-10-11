#include "Physics/PhysXConversions.h"

physx::PxVec3 ToPxVec3(const FVector& V)
{
	return physx::PxVec3(V.X, V.Y, V.Z);
}

FVector FromPxVec3(const physx::PxVec3& V)
{
	return FVector(V.x, V.y, V.z);
}

physx::PxQuat ToPxQuat(const FQuat& Q)
{
	return physx::PxQuat(Q.X, Q.Y, Q.Z, Q.W);
}

FQuat FromPxQuat(const physx::PxQuat& Q)
{
	return FQuat(Q.x, Q.y, Q.z, Q.w);
}

physx::PxTransform ToPxTransform(const FVector& Location, const FQuat& Rotation)
{
	return physx::PxTransform(ToPxVec3(Location), ToPxQuat(Rotation));
}

FTransform FromPxTransform(const physx::PxTransform& T)
{
	return FTransform(FromPxVec3(T.p), FromPxQuat(T.q), FVector::OneVector);
}
