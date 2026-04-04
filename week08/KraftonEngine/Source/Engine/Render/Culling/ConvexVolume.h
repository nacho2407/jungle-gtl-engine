#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

struct FConvexVolume
{
public:
	void UpdateFromMatrix(const FMatrix& InViewProjectionMatrix);
	bool IntersectAABB(const FBoundingBox& Box) const;

private:
	FVector4 Planes[6];
};