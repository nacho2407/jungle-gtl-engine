#include "Frustum.h"
#include "Component/PrimitiveComponent.h"

void FFrustum::ExtractFromVP(const FMatrix& VP)
{
	// Row-major VP: column extraction via M[row][col]
	// Left:   col3 + col0
	Planes[Left]   = { VP[0][3] + VP[0][0], VP[1][3] + VP[1][0], VP[2][3] + VP[2][0], VP[3][3] + VP[3][0] };
	// Right:  col3 - col0
	Planes[Right]  = { VP[0][3] - VP[0][0], VP[1][3] - VP[1][0], VP[2][3] - VP[2][0], VP[3][3] - VP[3][0] };		
	// Bottom: col3 + col1
	Planes[Bottom] = { VP[0][3] + VP[0][1], VP[1][3] + VP[1][1], VP[2][3] + VP[2][1], VP[3][3] + VP[3][1] };
	// Top:    col3 - col1
	Planes[Top]    = { VP[0][3] - VP[0][1], VP[1][3] - VP[1][1], VP[2][3] - VP[2][1], VP[3][3] - VP[3][1] };
	// Near:   col3 + col2
	Planes[Near]   = { VP[0][3] + VP[0][2], VP[1][3] + VP[1][2], VP[2][3] + VP[2][2], VP[3][3] + VP[3][2] };
	// Far:    col3 - col2
	Planes[Far]    = { VP[0][3] - VP[0][2], VP[1][3] - VP[1][2], VP[2][3] - VP[2][2], VP[3][3] - VP[3][2] };

	for (int32 i = 0; i < PlaneCount; ++i)
	{
		Planes[i].Normalize();
	}
}

bool FFrustum::IsVisible(const FBoxSphereBounds& Sphere) const
{
	for (int32 i = 0; i < PlaneCount; ++i)
	{
		if (Planes[i].DistanceTo(Sphere.Center) < -Sphere.Radius)
		{
			return false;
		}
	}
	return true;
}

FFrustum::EContainment FFrustum::TestAABB(const FVector& Center, const FVector& Extent) const
{
	bool bIntersects = false;

	for (int32 i = 0; i < PlaneCount; ++i)
	{
		const FPlane4& Plane = Planes[i];
		const float Distance = Plane.DistanceTo(Center);
		const float Radius =
			std::abs(Plane.A) * Extent.X +
			std::abs(Plane.B) * Extent.Y +
			std::abs(Plane.C) * Extent.Z;

		if (Distance < -Radius)
		{
			return EContainment::Outside;
		}

		if (Distance <= Radius)
		{
			bIntersects = true;
		}
	}

	return bIntersects ? EContainment::Intersecting : EContainment::Inside;
}
