#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Math/Matrix.h"

struct FBoundingBox;

struct FConvexVolume
{
public:
	void UpdateFromMatrix(const FMatrix& InViewProjectionMatrix);
	bool IntersectAABB(const FBoundingBox& Box) const;
	// Returns true if the AABB is completely inside all 6 frustum planes
	bool ContainsAABB(const FBoundingBox& Box) const;

private:
	FVector4 Planes[6];
};