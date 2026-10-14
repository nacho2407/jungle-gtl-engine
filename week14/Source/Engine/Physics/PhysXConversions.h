#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Physics/PhysXInclude.h"

physx::PxVec3 ToPxVec3(const FVector& V);
FVector FromPxVec3(const physx::PxVec3& V);

physx::PxQuat ToPxQuat(const FQuat& Q);
FQuat FromPxQuat(const physx::PxQuat& Q);

physx::PxTransform ToPxTransform(const FVector& Location, const FQuat& Rotation);
FTransform FromPxTransform(const physx::PxTransform& T);
