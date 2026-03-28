#pragma once
#include "PrimitiveBase.h"

class ENGINE_API FPrimitiveSky : public FPrimitiveBase
{
public:
	static const FString Key;


	FPrimitiveSky(int32 Segments = 32, int32 Rings = 32);

	void Generate(int32 Segments, int32 Rings);
};