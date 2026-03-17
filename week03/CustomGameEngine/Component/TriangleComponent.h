#pragma once
#include "PrimitiveComponent.h"

class UTriangleComp : public UPrimitiveComponent
{
	DECLARE_RTTI(UTriangleComp, UPrimitiveComponent)
public:
	UTriangleComp();
	virtual ~UTriangleComp() override;
};

