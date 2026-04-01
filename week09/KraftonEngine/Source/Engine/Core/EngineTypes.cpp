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

bool FBoundingBox::IsValid() const
{
	return Min.X <= Max.X && Min.Y <= Max.Y && Min.Z <= Max.Z;
}
