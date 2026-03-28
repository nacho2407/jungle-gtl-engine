#pragma once
#include "PrimitiveComponent.h"

class ENGINE_API UPlaneComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(UPlaneComponent, UPrimitiveComponent)

	void Initialize();
};
