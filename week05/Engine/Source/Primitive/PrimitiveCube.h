#pragma once

#include "Primitive/PrimitiveBase.h"

class ENGINE_API FPrimitiveCube : public FPrimitiveBase
{
public:
	static const FString Key;
	// static FString GetFilePath();

	FPrimitiveCube();

	// 파일 없이 코드로 직접 생성
	void Generate();
};
