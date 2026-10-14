#include "Physics/ConstraintInstance.h"

void FConstraintInstance::Release()
{
	if (Joint)
	{
		Joint->release();
		Joint = nullptr;
	}

	BodyA = nullptr;
	BodyB = nullptr;
}
