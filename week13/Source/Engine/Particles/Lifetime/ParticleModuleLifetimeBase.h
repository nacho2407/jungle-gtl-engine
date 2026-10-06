#pragma once
#include "Particles/ParticleModule.h"

#include "Source/Engine/Particles/Lifetime/ParticleModuleLifetimeBase.generated.h"

UCLASS()
class UParticleModuleLifetimeBase : public UParticleModule
{
public:
	GENERATED_BODY()

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
	virtual float GetLifetimeValue(const FContext& Context, float InTime, UObject* Data = NULL) { return 0.0f;  };
};
