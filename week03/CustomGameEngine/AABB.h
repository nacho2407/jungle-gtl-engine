#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"
struct FAABB
{
	FVector MinPos = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxPos = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	FMatrix RelativeMatrix = FMatrix::IdentityMatrix();
};