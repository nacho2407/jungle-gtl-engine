#pragma once

#include "Primitive/PrimitiveBase.h"

class ENGINE_API FPrimitivePlane : public FPrimitiveBase
{
public:
	static const FString Key;
	// static FString GetFilePath();

	FPrimitivePlane();

	void Generate();
};
