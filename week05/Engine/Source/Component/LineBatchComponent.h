#pragma once

#include "PrimitiveComponent.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "PrimitiveLineBatch.h"

class ULineBatchComponent : public UPrimitiveComponent
{
	DECLARE_RTTI(ULineBatchComponent, UPrimitiveComponent)

public:
	void Initialize();
	void DrawLine(FVector InStart, FVector InEnd, FVector4 color);
	void DrawWireCube(FVector InCenter, FQuat InRotation, FVector InScale, FVector4 InColor);
	void DrawWireSphere(FVector InCenter, float InRadius, FVector4 InColor);
	void Clear();
};