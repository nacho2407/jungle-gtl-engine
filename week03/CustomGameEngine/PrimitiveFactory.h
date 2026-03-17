#pragma once
#include "PrimitiveComponent.h"

struct PrimitiveFactory
{
	static UPrimitiveComponent* AddPrimitive(EPrimitiveType type);
};

