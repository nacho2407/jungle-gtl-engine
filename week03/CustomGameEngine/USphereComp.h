#pragma once
#include "UPrimitiveComponent.h"

class USphereComp : public UPrimitiveComponent
{
	DECLARE_RTTI(USphereComp, UPrimitiveComponent)
public:
	USphereComp();
	virtual ~USphereComp() override;
	virtual URenderObject* Render() override;
};
