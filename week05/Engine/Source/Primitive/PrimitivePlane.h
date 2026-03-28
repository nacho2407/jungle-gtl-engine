#pragma once

#include "Primitive/PrimitiveBase.h"

class ENGINE_API CPrimitivePlane : public CPrimitiveBase
{
public:
	static const FString Key;
	// static FString GetFilePath();

	CPrimitivePlane();

	void Generate();
};
