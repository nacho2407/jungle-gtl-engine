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
