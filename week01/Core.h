#pragma once

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
	float u, v;
};

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

	FVector operator +(const FVector& rhs) const
	{
		return FVector(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	FVector operator -(const FVector& rhs) const
	{
		return FVector(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	FVector operator *(float scala) const
	{
		return FVector(x * scala, y * scala, z * scala);
	}

	float Dot2(const FVector& rhs) const
	{
		return x * rhs.x + y * rhs.y;
	}

	static FVector Splat(float Scalar)
	{
		return FVector(Scalar, Scalar, Scalar);
	}
};
