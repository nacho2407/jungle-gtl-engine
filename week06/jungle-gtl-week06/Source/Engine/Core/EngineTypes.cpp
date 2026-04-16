#include "EngineTypes.h"

void FBoundingBox::Expand(const FVector& Point)
{
	if (Point.X < Min.X) Min.X = Point.X;
	if (Point.Y < Min.Y) Min.Y = Point.Y;
	if (Point.Z < Min.Z) Min.Z = Point.Z;
	if (Point.X > Max.X) Max.X = Point.X;
	if (Point.Y > Max.Y) Max.Y = Point.Y;
	if (Point.Z > Max.Z) Max.Z = Point.Z;
}

FVector FBoundingBox::GetCenter() const
{
	return FVector(
		(Min.X + Max.X) * 0.5f,
		(Min.Y + Max.Y) * 0.5f,
		(Min.Z + Max.Z) * 0.5f
	);
}

FVector FBoundingBox::GetExtent() const
{
	return FVector(
		(Max.X - Min.X) * 0.5f,
		(Max.Y - Min.Y) * 0.5f,
		(Max.Z - Min.Z) * 0.5f
	);
}

void FBoundingBox::GetCorners(FVector (&OutCorners)[8]) const
{
	OutCorners[0] = FVector(Min.X, Min.Y, Min.Z);
	OutCorners[1] = FVector(Max.X, Min.Y, Min.Z);
	OutCorners[2] = FVector(Min.X, Max.Y, Min.Z);
	OutCorners[3] = FVector(Max.X, Max.Y, Min.Z);
	OutCorners[4] = FVector(Min.X, Min.Y, Max.Z);
	OutCorners[5] = FVector(Max.X, Min.Y, Max.Z);
	OutCorners[6] = FVector(Min.X, Max.Y, Max.Z);
	OutCorners[7] = FVector(Max.X, Max.Y, Max.Z);
}

bool FBoundingBox::IsValid() const
{
	return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}
