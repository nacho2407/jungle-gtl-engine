#pragma once

#include "Serializable.h"

struct FMatrix;

// Structure for a 3D vector
class FVector : public ISerializable
{
public:
    FVector(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
	~FVector();
    float Dot(const FVector& Other) const;
    FVector Cross(const FVector& Other) const;
    FVector operator+(const FVector& Other) const;
    FVector operator-(const FVector& Other) const;
    FVector operator*(const float s) const;
    FVector operator/(const float s) const;

    FVector Normalize() const;
    float Length() const;
    bool IsNearlyEqual(const FVector& Other, float Epsilon = 1e-6f) const;
	FVector Transform(const FMatrix &Mat);

	static FVector Zero() { return FVector(0.0f, 0.0f, 0.0f); }
	static FVector Up() { return FVector(0.0f, 0.0f, 1.0f); }
	static FVector Right() { return FVector(0.0f, 1.0f, 0.0f); }
	static FVector Forward() { return FVector(1.0f, 0.0f, 0.0f); }

	json::JSON Serialize();
	void Deserialize(json::JSON);
public:
	float x, y, z;
};

inline float Dot(const FVector& a, const FVector& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline FVector Add(const FVector& a, const FVector& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline FVector Sub(const FVector& a, const FVector& b)
{
	return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline FVector Mul(const FVector& v, float s)
{
	return { v.x * s, v.y * s, v.z * s };
}

inline FVector Div(const FVector& v, float s)
{
	if (s == 0.0f)
	{
		// throw std::invalid_argument("Division by zero condition!");
		return { 0.0f, 0.0f, 0.0f };
	}
	return { v.x / s, v.y / s, v.z / s };
}

FVector Cross(const FVector& a, const FVector& b);