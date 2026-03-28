#pragma once
#include "PrimitiveComponent.h"

class ENGINE_API USphereComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(USphereComponent, UPrimitiveComponent)

	void Initialize();
};
