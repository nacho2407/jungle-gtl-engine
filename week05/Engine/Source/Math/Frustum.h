#pragma once
#include "CoreMinimal.h"
#include <cmath>

struct FBoxSphereBounds;

struct FPlane4
{
	float A, B, C, D;

	void Normalize()
	{
		float Len = std::sqrt(A * A + B * B + C * C);
		if (Len > 0.0f)
		{
			A /= Len;
			B /= Len;
			C /= Len;
			D /= Len;
		}
	}

	float DistanceTo(const FVector& Point) const
	{
		return A * Point.X + B * Point.Y + C * Point.Z + D;
	}
};

struct FBoundingSphere
{
	FVector Center;
	float Radius;
};

class ENGINE_API FFrustum
{
public:
	enum { Left = 0, Right, Bottom, Top, Near, Far, PlaneCount };

	void ExtractFromVP(const FMatrix& VP);
	bool IsVisible(const FBoxSphereBounds& Sphere) const;

private:
	FPlane4 Planes[PlaneCount];
};
