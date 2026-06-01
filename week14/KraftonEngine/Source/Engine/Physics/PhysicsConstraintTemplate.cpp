#include "PhysicsConstraintTemplate.h"
#include "Serialization/Archive.h"

void UPhysicsConstraintTemplate::Serialize(FArchive& Ar)
{
	Ar << ParentBoneName;
	Ar << ChildBoneName;

	Ar << LocalFrameA;
	Ar << LocalFrameB;

	Ar << AngularMode;
	Ar << Swing1Limit;
	Ar << Swing2Limit;
	Ar << TwistLimit;
}

void UPhysicsConstraintTemplate::SetFrames(const FTransform& InFrameA, const FTransform& InFrameB)
{
	LocalFrameA = InFrameA;
	LocalFrameB = InFrameB;
}

void UPhysicsConstraintTemplate::SetLocalFrameA(const FTransform& InFrameA)
{
	LocalFrameA = InFrameA;
}

void UPhysicsConstraintTemplate::SetLocalFrameB(const FTransform& InFrameB)
{
	LocalFrameB = InFrameB;
}

void UPhysicsConstraintTemplate::SetAngularMode(EAngularConstraintMode InMode)
{
	AngularMode = InMode;
}

bool UPhysicsConstraintTemplate::SetAngularLimits(float InSwing1Limit, float InSwing2Limit, float InTwistLimit)
{
	if (InSwing1Limit < 0.0f || InSwing2Limit < 0.0f || InTwistLimit < 0.0f)
	{
		return false;
	}

	Swing1Limit = InSwing1Limit;
	Swing2Limit = InSwing2Limit;
	TwistLimit = InTwistLimit;
	return true;
}

bool UPhysicsConstraintTemplate::SetAngularLimit(EAngularConstraintMode InMode, float InSwing1Limit, float InSwing2Limit, float InTwistLimit)
{
	if (!SetAngularLimits(InSwing1Limit, InSwing2Limit, InTwistLimit))
	{
		return false;
	}

	AngularMode = InMode;
	return true;
}