#pragma once
#include <cmath>
#include <cstring>
#include "Vector.h"

struct FMatrix {
	static const FMatrix Identity;

	union
	{
		float M[4][4];

		// Iteration 가능 +Cache 친화
		float Data[16];

		//추후 성능 개선을 위해 SIMD 명령어로 4개의 행을 한 번에 처리할 수 있도록 FVectors4 배열로도 접근 가능하게 할 수 있음
		//FVectors4 Rows[4];	
	};

	// Default constructor (Zero matrix)
	FMatrix() {
		std::memset(Data, 0, sizeof(Data));
	}

	FMatrix(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		M[0][0] = m00; M[0][1] = m01; M[0][2] = m02; M[0][3] = m03;
		M[1][0] = m10; M[1][1] = m11; M[1][2] = m12; M[1][3] = m13;
		M[2][0] = m20; M[2][1] = m21; M[2][2] = m22; M[2][3] = m23;
		M[3][0] = m30; M[3][1] = m31; M[3][2] = m32; M[3][3] = m33;
	}

	FMatrix(const FMatrix& other) {
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				M[i][j] = other.M[i][j];
			}
		}
	}

	FMatrix operator+(const FMatrix& Other) const;
	FMatrix operator-(const FMatrix& Other) const;
	FMatrix operator*(const FMatrix& Other) const;
	FMatrix operator/(float Scalar) const;

	FMatrix operator+(float Scalar) const;
	FMatrix operator-(float Scalar) const;
	FMatrix operator*(float Scalar) const;

	FMatrix& operator+=(const FMatrix& Other);
	FMatrix& operator-=(const FMatrix& Other);
	FMatrix& operator*=(const FMatrix& Other);
	FMatrix& operator/=(float Scalar);

	FMatrix& operator+=(float Scalar);
	FMatrix& operator-=(float Scalar);
	FMatrix& operator*=(float Scalar);

	FMatrix GetTransposed() const;

	// For projection or matrices with scaling
	FMatrix GetInverse() const;

	// Fast path for orthonormal rotation + translation matrices (e.g. view matrix)
	FMatrix GetInverseFast() const;

	// Checks if the matrix is an approximate equivalent
	bool Equals(const FMatrix& other) const;

	// Checks if the matrix is an approximate identity
	bool IsIdentity() const;

	static FMatrix MakeTranslationMatrix(const FVector& Location);
	static FMatrix MakeScaleMatrix(const FVector& Scale);
	static FMatrix MakeRotationEuler(const FVector& Rotation);
	static FMatrix MakeRotationAxis(const FVector& Axis, float Angle);

	static FMatrix MakeRotationX(float Angle);
	static FMatrix MakeRotationY(float Angle);
	static FMatrix MakeRotationZ(float Angle);

	static FMatrix GetCancelRotationMatrix(const FMatrix& InMatrix);
	void Print() const;

	FVector TransformVector(const FVector& vector) const;
	FVector TransformPositionWithW(const FVector& V) const;

	FVector GetEuler() const;
	FVector GetLocation() const;
};

FVector operator* (const FVector& vector, const FMatrix& matrix);