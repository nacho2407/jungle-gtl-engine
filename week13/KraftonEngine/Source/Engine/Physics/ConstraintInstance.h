#pragma once
#include <extensions/PxD6Joint.h>

#include "Math/Transform.h"
#include "Object/FName.h"

struct FBodyInstance;

struct FConstraintInstanceInitDesc
{
	FBodyInstance* ParentBody = nullptr;
	FBodyInstance* ChildBody = nullptr;

	FName ParentBoneName = FName::None;
	FName ChildBoneName = FName::None;

	FTransform ParentFrame;
	FTransform ChildFrame;

	float TwistLimitDegrees = 45.0f;
	float Swing1LimitDegrees = 30.0f;
	float Swing2LimitDegrees = 30.0f;

	bool bEnableCollision = false;
};

struct FConstraintInstance
{
	FBodyInstance* ParentBody = nullptr;
	FBodyInstance* ChildBody = nullptr;

	FName ParentBoneName = FName::None;
	FName ChildBoneName = FName::None;

	FTransform ParentFrame;
	FTransform ChildFrame;

	float TwistLimitDegrees = 45.0f;
	float Swing1LimitDegrees = 30.0f;
	float Swing2LimitDegrees = 30.0f;

	bool bEnableCollision = false;

	physx::PxD6Joint* Joint = nullptr;

	bool IsValidConstraintInstance() const { return Joint != nullptr; }
	void ClearPhysicsPointers() { Joint = nullptr; }

	float GetTwistLimitRadians() const;
	float GetSwing1LimitRadians() const;
	float GetSwing2LimitRadians() const;
};

