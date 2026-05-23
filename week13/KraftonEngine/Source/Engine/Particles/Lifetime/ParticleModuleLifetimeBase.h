#pragma once
#include "Particles/ParticleModule.h"

class UParticleModuleLifetimeBase : public UParticleModule
{
public:
	virtual float	GetMaxLifetime()
	{
		return 0.0f;
	}

	/**
	 *	Return the lifetime value at the given time.
	 *
	 *	@param	Owner		The emitter instance that owns this module
	 *	@param	InTime		The time input for retrieving the lifetime value
	 *	@param	Data		The data associated with the distribution
	 *
	 *	@return	float		The Lifetime value
	 */
	virtual float GetLifetimeValue(const FContext& Context, float InTime, UObject* Data = NULL) = 0;
};
