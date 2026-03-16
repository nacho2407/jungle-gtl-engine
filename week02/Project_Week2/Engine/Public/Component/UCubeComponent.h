#pragma once
#include "Component/UPrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
public:
	UCubeComponent() 
	{
		PrimitiveType = EPrimitiveType::Cube;
	}
};