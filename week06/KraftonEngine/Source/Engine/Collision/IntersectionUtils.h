#pragma once

#include "Core/EngineTypes.h"
#include "Math/Matrix.h"

struct FIntersectionUtils
{
	static bool IntersectOBBAndAABB(const FMatrix& OBBWorldMatrix, const FVector& OBBHalfExtents, const FBoundingBox& AABB);
};
