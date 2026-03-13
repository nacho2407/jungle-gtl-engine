#pragma once
#include <cmath>
struct FMatrix4x4 {
public:
	float m[4][4];
	FMatrix4x4() {
		for (int i{ 0 }; i < 4; i++)
			for (int j{ 0 }; j < 4; j++) m[i][j] = (i == j) ? 1.0f : 0.0f;
	}

public:
	static FMatrix4x4 IdentityMatrix()
	{
		return FMatrix4x4();
	}

	static FMatrix4x4 ZeroMatrix();
	static FMatrix4x4 TranslationMatrix(float InX = 0.f, float InY = 0.f, float InZ = 0.f);

	static FMatrix4x4 ScaleMatrix(float InX = 1.f, float InY = 1.f, float InZ = 1.f);

	static float DegToRad(float Degree) { return Degree * 3.14159265358979323846f / 180.0f; }

	//Euler Angle
	static FMatrix4x4 RotationXMatrix(float InXDegree);

	static FMatrix4x4 RotationYMatrix(float InYDegree);

	static FMatrix4x4 RotationZMatrix(float InZDegree);

	//X->Y->Z
	static FMatrix4x4 EulerRotationMatrix(float InX = 0.f, float InY = 0.f, float InZ = 0.f);

	FMatrix4x4 operator*(const FMatrix4x4& Other) const;


	float* operator[](int Row) { return m[Row]; }

	const float* operator[](int Row)const { return m[Row]; }
};