#pragma once
#include "Component/UPrimitiveComponent.h"

class UTriangleComponent : public UPrimitiveComponent
{
	DECLARE_UClass(UTriangleComponent, UPrimitiveComponent)

public:
	UTriangleComponent()
	{
		//PrimitiveType = EPrimitiveType::Triangle;
	}
};