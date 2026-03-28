#pragma once
#include "PrimitiveComponent.h"

class ENGINE_API UCubeComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UCubeComponent, UPrimitiveComponent)
	void Initialize();
};
