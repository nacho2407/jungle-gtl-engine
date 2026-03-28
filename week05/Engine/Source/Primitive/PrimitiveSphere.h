#pragma once

#include "Primitive/PrimitiveBase.h"

class ENGINE_API FPrimitiveSphere : public FPrimitiveBase
{
public:
	static const FString Key;
	// static FString GetFilePath();

	FPrimitiveSphere(int32 Segments = 16, int32 Rings = 16);

	void Generate(int32 Segments, int32 Rings);
};
