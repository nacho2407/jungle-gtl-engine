#pragma once

#include "Math/FVector.h"
#include "Math/FMatrix.h"

struct FQuat
{
	float X;
	float Y;
	float Z;
	float W;

public:
	FQuat();
	FQuat(float InX, float InY, float InZ, float InW);

	static FQuat Identity();

	static FQuat FromAxisAngle(const FVector& Axis, float Radians);
	static FQuat FromEulerXYZ(const FVector& EulerRadians);
	FVector ToEulerXYZ() const;

	void Normalize();
	FQuat GetNormalized() const;
	FQuat Conjugate() const;

	FMatrix ToMatrix() const;

public:
	FQuat operator*(const FQuat& Rhs) const;
};