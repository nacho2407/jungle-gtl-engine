#pragma once
#include "Math/FVector.h"
#include "Math/FVector4.h"

struct FMatrix
{
	float M[4][4];

public:
	FMatrix();

	static const FMatrix Identity;

public:
	static FMatrix MakeIdentity();
	static FMatrix MakeTranslation(const FVector& Translation);
	static FMatrix MakeScale(const FVector& Scale);
	static FMatrix MakeRotationX(float Radians);
	static FMatrix MakeRotationY(float Radians);
	static FMatrix MakeRotationZ(float Radians);
	static FMatrix MakeRotationXYZ(const FVector& RadiansXYZ);

	static FMatrix MakeViewMatrix(const FVector& CameraLocation, const FVector& CameraRotation);
	static FMatrix MakePerspectiveMatrix(float FOVDegrees, float AspectRatio, float NearClip, float FarClip);
	static FMatrix MakeOrthogonalMatrix(float OrthWidth, float AspectRatio, float NearClip, float FarClip);

	static FMatrix Transpose(const FMatrix& Mat);

	static FMatrix InverseAffine(const FMatrix& Mat);

public:
	FMatrix operator*(const FMatrix& Rhs) const;
	FVector4 operator*(const FVector4& Vec) const;

	FVector TransformPosition(const FVector& Vec) const;
	FVector TransformVector(const FVector& Vec) const;

	FVector GetTranslation() const;
	void SetTranslation(const FVector& Translation);
};