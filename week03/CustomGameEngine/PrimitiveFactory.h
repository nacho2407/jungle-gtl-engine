#pragma once
#include "UPrimitiveComponent.h"

struct PrimitiveFactory
{
	static UPrimitiveComponent* AddPrimitive(EPrimitiveType type);
};

