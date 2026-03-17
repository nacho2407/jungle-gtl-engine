#pragma once
#include "PrimitiveComponent.h"

class USphereComp : public UPrimitiveComponent
{
	DECLARE_RTTI(USphereComp, UPrimitiveComponent)
public:
	USphereComp();
	virtual ~USphereComp() override;
};
