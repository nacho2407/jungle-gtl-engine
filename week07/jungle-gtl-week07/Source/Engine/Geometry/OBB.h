#pragma once

#include "Core/Containers/Array.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

struct FOBB
{
	FVector Center;
	FVector Extents;
	FQuat Rotation;

	FOBB();
	FOBB(const FVector& InCenter, const FVector& InExtents, const FQuat& InRotation);
	FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InMatrix);

	void Reset();
	bool IsValid() const;

	static FOBB FromAABB(const FAABB& InAABB, const FMatrix& InTransform);

	inline void GetAxes(FVector& OutX, FVector& OutY, FVector& OutZ) const;
	inline void GetVertices(TArray<FVector>& OutVertices) const;
	inline FMatrix GetTransform() const;

	inline bool Contains(const FVector& Point) const;

	inline bool Intersects(const FAABB& AABB) const;
};

inline void FOBB::GetAxes(FVector& OutX, FVector& OutY, FVector& OutZ) const
{
	const FMatrix RotMat = Rotation.ToMatrix();
	OutX = RotMat.GetScaledAxis(EAxis::X);
	OutY = RotMat.GetScaledAxis(EAxis::Y);
	OutZ = RotMat.GetScaledAxis(EAxis::Z);
}

inline void FOBB::GetVertices(TArray<FVector>& OutVertices) const
{
	FVector X, Y, Z;
	GetAxes(X, Y, Z);

	X *= Extents.X;
	Y *= Extents.Y;
	Z *= Extents.Z;

	OutVertices.resize(8);
	OutVertices[0] = Center - X - Y - Z;
	OutVertices[1] = Center + X - Y - Z;
	OutVertices[2] = Center - X + Y - Z;
	OutVertices[3] = Center + X + Y - Z;
	OutVertices[4] = Center - X - Y + Z;
	OutVertices[5] = Center + X - Y + Z;
	OutVertices[6] = Center - X + Y + Z;
	OutVertices[7] = Center + X + Y + Z;
}

inline FMatrix FOBB::GetTransform() const
{
	const FMatrix RotMat = Rotation.ToMatrix();
	FMatrix Transform = RotMat;
	Transform.SetOrigin(Center);
	return Transform;
}

inline bool FOBB::Contains(const FVector& Point) const
{
	FVector LocalPoint = Rotation.Inverse() * (Point - Center);
	return MathUtil::Abs(LocalPoint.X) <= Extents.X && MathUtil::Abs(LocalPoint.Y) <= Extents.Y &&
		MathUtil::Abs(LocalPoint.Z) <= Extents.Z;
}

inline bool FOBB::Intersects(const FAABB& AABB) const
{
	// Separating Axis Theorem (SAT) for OBB-AABB intersection
	FVector OBBAxes[3]; GetAxes(OBBAxes[0], OBBAxes[1], OBBAxes[2]);
	FVector AABBAxes[3] = { FVector::UnitX(), FVector::UnitY(), FVector::UnitZ() };
	
	FVector OBBExtents = Extents;
	FVector AABBExtents = AABB.GetExtent();

	FVector T = Center - AABB.GetCenter();

	// AABB axes
	for (int i = 0; i < 3; ++i)
	{
		float rA = AABBExtents[i];
		float rB = OBBExtents.X * MathUtil::Abs(OBBAxes[0][i]) +
			OBBExtents.Y * MathUtil::Abs(OBBAxes[1][i]) +
			OBBExtents.Z * MathUtil::Abs(OBBAxes[2][i]);
		if (MathUtil::Abs(T[i]) > rA + rB) return false;
	}

	// OBB axes
	for (int i = 0; i < 3; ++i)
	{
		float rA = AABBExtents.X * MathUtil::Abs(OBBAxes[i].X) +
			AABBExtents.Y * MathUtil::Abs(OBBAxes[i].Y) +
			AABBExtents.Z * MathUtil::Abs(OBBAxes[i].Z);
		float rB = OBBExtents[i];
		if (MathUtil::Abs(T.DotProduct(OBBAxes[i])) > rA + rB) return false; 
	}

	// AABB cross OBB axes
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			FVector L;
			if (i == 0) L = FVector(0, -OBBAxes[j].Z, OBBAxes[j].Y);
			else if (i == 1) L = FVector(OBBAxes[j].Z, 0, -OBBAxes[j].X);
			else L = FVector(-OBBAxes[j].Y, OBBAxes[j].X, 0);

			if (L.SizeSquared() > MathUtil::Epsilon)
			{
				float rA = AABBExtents[(i + 1) % 3] * MathUtil::Abs(L[(i + 1) % 3]) +
					AABBExtents[(i + 2) % 3] * MathUtil::Abs(L[(i + 2) % 3]);

				float rB = OBBExtents.X * MathUtil::Abs(L.DotProduct(OBBAxes[0])) +
					OBBExtents.Y * MathUtil::Abs(L.DotProduct(OBBAxes[1])) +
					OBBExtents.Z * MathUtil::Abs(L.DotProduct(OBBAxes[2]));

				if (MathUtil::Abs(T.DotProduct(L)) > rA + rB) return false;
			}
		}
	}

	return true;
}
