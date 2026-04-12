#pragma once

#include "Core/EngineTypes.h"
#include "Math/Matrix.h"

struct FIntersectionUtils
{
	bool IntersectOBBAndAABB(const FMatrix& OBBWorldMatrix, const FVector& OBBHalfExtents, const FBoundingBox& AABB);
}
