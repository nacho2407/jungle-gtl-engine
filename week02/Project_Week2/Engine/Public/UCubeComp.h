#pragma once
#include "UObject.h"
#include "UPrimitiveComponent.h"

class UCubeComp : public UPrimitiveComponent
{
public:
	UCubeComp();

	virtual const char* GetTypeString() const override
	{
		return "Cube";
	}
};