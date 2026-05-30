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