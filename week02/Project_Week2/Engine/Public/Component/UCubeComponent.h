#pragma once
#include "Component/UPrimitiveComponent.h"

class UCubeComponent : public UPrimitiveComponent
{
	DECLARE_UClass(UCubeComponent, UPrimitiveComponent)

public:
	UCubeComponent() 
	{
		//PrimitiveType = EPrimitiveType::Cube;
	}
};