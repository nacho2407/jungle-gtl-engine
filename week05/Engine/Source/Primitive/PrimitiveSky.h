#pragma once
#include "PrimitiveBase.h"

class ENGINE_API CPrimitiveSky : public CPrimitiveBase
{
public:
	static const FString Key;


	CPrimitiveSky(int32 Segments = 32, int32 Rings = 32);

	void Generate(int32 Segments, int32 Rings);
};