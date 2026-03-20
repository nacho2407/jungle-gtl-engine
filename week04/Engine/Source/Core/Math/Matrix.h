#pragma once

#include "../EngineAPI.h"
#include "Vector.h"

struct ENGINE_API FMatrix {
public:
	float m[4][4];
	FMatrix() {
		for (int i{ 0 }; i < 4; i++)
			for (int j{ 0 }; j < 4; j++) m[i][j] = (i == j) ? 1.0f : 0.0f;
	}

public:
	static FMatrix IdentityMatrix()
	{
		return FMatrix();
	}

	static FMatrix ZeroMatrix();
	static FMatrix TranslationMatrix(FVector InVec) { return TranslationMatrix(InVec.x, InVec.y, InVec.z); }
	static FMatrix TranslationMatrix(float InX = 0.f, float InY = 0.f, float InZ = 0.f);

	static FMatrix ScaleMatrix(FVector InVec) { return ScaleMatrix(InVec.x, InVec.y, InVec.z); }
	static FMatrix ScaleMatrix(float InX = 1.f, float InY = 1.f, float InZ = 1.f);

	FVector TransformPoint(const FVector& Vector) const;
	FVector TransformVector(const FVector& Vector) const;

	static float DegToRad(float Degree) { return Degree * 3.14159265358979323846f / 180.0f; }

	//Euler Angle
	static FMatrix RotationXMatrix(float InRoll);

	static FMatrix RotationYMatrix(float InPitch);

	static FMatrix RotationZMatrix(float InYaw);

	static FMatrix EulerRotationMatrix(const FRotator& InRot);

	//X->Y->Z
	static FMatrix EulerRotationMatrix(float InRoll = 0.0f, float InPitch = 0.0f, float InYaw = 0.0f);

	FMatrix operator*(const FMatrix& Other) const;

	static FMatrix ViewMatrix(FVector Eye, FVector Target, FVector Up);

	FMatrix PerspectiveMatrix(float FovDegree, float Aspect, float Near, float Far);

	FMatrix Inverse() const;

	float* operator[](int Row) { return m[Row]; }

	const float* operator[](int Row)const { return m[Row]; }
};
