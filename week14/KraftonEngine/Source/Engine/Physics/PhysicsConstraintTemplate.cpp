#include "PhysicsConstraintTemplate.h"
#include "Serialization/Archive.h"
#include <algorithm>

void UPhysicsConstraintTemplate::SetAngularLimits(float InSwing1Limit, float InSwing2Limit, float InTwistLimit)
{
	Swing1Limit = std::clamp(InSwing1Limit, 0.0f, 180.0f);
	Swing2Limit = std::clamp(InSwing2Limit, 0.0f, 180.0f);
	TwistLimit = std::clamp(InTwistLimit, 0.0f, 180.0f);
}

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
