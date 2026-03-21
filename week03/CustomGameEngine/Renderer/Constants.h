#pragma once
#include "Math/Matrix.h"
#include "EngineCore/Color.h"
struct FObjectConstant
{
	FMatrix World;
	FColor Color;
	int bIsSelected;
	float Padding[3];
};