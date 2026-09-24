#pragma once

#include "Component/PrimitiveComponent.h"

struct PrimitiveFactory
{
	static UPrimitiveComponent* AddPrimitive(EPrimitiveType type);
};

