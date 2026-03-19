#pragma once

struct FMatrix;

struct  FVector
{
public:
	FVector(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f);
	~FVector();
	float Dot(const FVector& Other) const;
	FVector Cross(const FVector& Other) const;
	FVector operator+(const FVector& Other) const;
	FVector operator-(const FVector& Other) const;
	FVector operator*(const float s) const;
	FVector operator/(const float s) const;

	FVector Normalize() const;
	float Length() const;

	bool IsNearlyEqual(const FVector& Other) const;
	bool operator==(const FVector& Other) const;

	static FVector Zero();
	static FVector Up();
	static FVector Right();
	static FVector Forward();

public:
	float x, y, z;
};

