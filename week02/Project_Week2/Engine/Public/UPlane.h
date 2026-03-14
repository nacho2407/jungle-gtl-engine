#pragma once
#include "UPrimitiveComponent.h"

class UPlaneComp : public UPrimitiveComponent
{
public:
	UPlaneComp();

	virtual const char* GetTypeString() const override
	{
		return "Plane";
	}
};