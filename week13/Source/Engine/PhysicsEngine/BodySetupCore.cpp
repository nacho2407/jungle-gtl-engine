#include "BodySetupCore.h"

ECollisionTraceFlag UBodySetupCore::GetCollisionTraceFlag() const
{
	if (CollisionTraceFlag == ECollisionTraceFlag::CTF_UseDefault)
	{
		return ECollisionTraceFlag::CTF_UseSimpleAndComplex;
	}

	return CollisionTraceFlag;
}
