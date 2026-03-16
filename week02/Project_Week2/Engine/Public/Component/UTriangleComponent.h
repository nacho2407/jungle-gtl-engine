#pragma once
#include "Component/UPrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
public:
	UTriangleComponent()
	{
		PrimitiveType = EPrimitiveType::Triangle;
	}
};