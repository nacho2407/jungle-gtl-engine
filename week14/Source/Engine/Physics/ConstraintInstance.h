#pragma once

#include "Physics/PhysXInclude.h"
#include "Math/Transform.h"

struct FBodyInstance;

struct FConstraintInstance
{
	FBodyInstance* BodyA = nullptr;
	FBodyInstance* BodyB = nullptr;

	FTransform LocalFrameA;
	FTransform LocalFrameB;

	physx::PxJoint* Joint = nullptr;

	bool IsValidConstraint() const { return Joint != nullptr; }

	void Release();
};
