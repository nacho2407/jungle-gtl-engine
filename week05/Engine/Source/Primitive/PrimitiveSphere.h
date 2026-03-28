#pragma once

#include "Primitive/PrimitiveBase.h"

class ENGINE_API CPrimitiveSphere : public CPrimitiveBase
{
public:
	static const FString Key;
	// static FString GetFilePath();

	CPrimitiveSphere(int32 Segments = 16, int32 Rings = 16);

	void Generate(int32 Segments, int32 Rings);
};
