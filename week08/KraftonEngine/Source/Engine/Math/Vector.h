#pragma once
#include <cmath>

#if defined(_MSC_VER)
	#include <intrin.h>     // MSVC
#else
	#include <immintrin.h>  // GCC / Clang
#endif

//	Virtual Table 용량을 줄이기 위해 Vector 인터페이스를 제거하고 FVector와 FVector4가 직접 구현하도록 변경
//struct Vector {
//	virtual float Length() const = 0;
//	virtual void Normalize() = 0;
//};

struct FVector {
	union
	{
		struct {
			float X, Y, Z;
		};

		struct
		{
			float R, G, B;
		};

		//	Iteration 가능 + Cache 친화
		float Data[3];
	};

	FVector() {
		X = 0.0f;
		Y = 0.0f;
		Z = 0.0f;
	}
	FVector(float InX, float InY, float InZ) {
		X = InX;
		Y = InY;
		Z = InZ;
	}
	
    static FVector ZeroVector;

	float Length() const;
	void  Normalize();
	FVector Normalized() const;

	float   Dot(const FVector& Other) const;
	FVector Cross(const FVector& Other) const;
	static FVector Cross(const FVector& v1, const FVector& v2) { return v1.Cross(v2); }
	static float Distance(const FVector& V1, const FVector& V2);
	static float DistSquared(const FVector& V1, const FVector& V2);

	FVector operator+(const FVector& Other) const;
	FVector operator-(const FVector& Other) const;
	FVector operator+(float Scalar) const;
	FVector operator-(float Scalar) const;
	FVector operator*(float Scalar) const;
	FVector operator/(float Scalar) const;

	FVector& operator+=(const FVector& Other);
	FVector& operator-=(const FVector& Other);
	FVector& operator+=(float Scalar);
	FVector& operator-=(float Scalar);
	FVector& operator*=(float Scalar);
	FVector& operator/=(float Scalar);
};

struct FVector4 {
	
	union
	{
		struct
		{
			float X, Y, Z, W;
		};
		struct 
		{
			float R, G, B, A;
		};

		//	Iteration 가능 + Cache 친화
		float Data[4];
	};

	FVector4() {
		X = 0.0f;
		Y = 0.0f;
		Z = 0.0f;
		W = 0.0f;
	}

	FVector4(float InX, float InY, float InZ, float InW) {
		X = InX;
		Y = InY;
		Z = InZ;
		W = InW;
	}

	FVector4(const FVector& Other, float InW) {
		X = Other.X;
		Y = Other.Y;
		Z = Other.Z;
		W = InW;
	}

	// Implicitly sets w = 1.0f
	FVector4(const FVector& Other) {
		X = Other.X;
		Y = Other.Y;
		Z = Other.Z;
		W = 1.0f;
	}

	float Length() const;
	void Normalize();
	FVector4 Normalized() const;

	float Dot(const FVector4& Other) const;

	// NOT a true cross vector (Drops w)
	FVector4 Cross(const FVector4& Other) const;
	static FVector4 Cross(const FVector4& v1, const FVector4& v2) { return v1.Cross(v2); }

	FVector4 operator+(const FVector4& Other) const;
	FVector4 operator-(const FVector4& other) const;
	FVector4 operator+(float Scalar) const;
	FVector4 operator-(float Scalar) const;
	FVector4 operator*(float Scalar) const;
	FVector4 operator/(float Scalar) const;

	FVector4& operator+=(const FVector4& Other);
	FVector4& operator-=(const FVector4& Other);
	FVector4& operator+=(float Scalar);
	FVector4& operator-=(float Scalar);
	FVector4& operator*=(float Scalar);
	FVector4& operator/=(float Scalar);

	static FVector rotateX(float rad, const FVector& vec) {
		auto cos_theta = cosf(rad);
		auto sin_theta = sinf(rad);
		FVector ret;
		ret.X = vec.X;
		ret.Y = cos_theta * vec.Y - sin_theta * vec.Z;
		ret.Z = sin_theta * vec.Y + cos_theta * vec.Z;
		return ret;
	}

	static FVector rotateY(float rad, const FVector& vec) {
		auto cos_theta = cosf(rad);
		auto sin_theta = sinf(rad);
		FVector ret;
		ret.X = cos_theta * vec.X + sin_theta * vec.Z;
		ret.Y = vec.Y;
		ret.Z = -sin_theta * vec.X + cos_theta * vec.Z;

		return ret;
	}

	static FVector rotateZ(float rad, const FVector& vec) {
		auto cos_theta = cosf(rad);
		auto sin_theta = sinf(rad);
		FVector ret;
		ret.X = cos_theta * vec.X - sin_theta * vec.Y;
		ret.Y = sin_theta * vec.X + cos_theta * vec.Y;
		ret.Z = vec.Z;

		return ret;
	}
};

struct FVector2
{
	union
	{
		struct
		{
			float X, Y;
		};
		struct
		{
			float U, V;
		};
		float Data[2];
	};
	FVector2() {
		X = 0.0f;
		Y = 0.0f;
	}
	FVector2(float InX, float InY) {
		X = InX;
		Y = InY;
	}
	float Length() const;
	void Normalize();
	FVector2 Normalized() const;
	float Dot(const FVector2& Other) const;
	FVector2 operator+(const FVector2& Other) const;
	FVector2 operator-(const FVector2& Other) const;
	FVector2 operator+(float Scalar) const;
	FVector2 operator-(float Scalar) const;
	FVector2 operator*(float Scalar) const;
	FVector2 operator/(float Scalar) const;
	FVector2& operator+=(const FVector2& Other);
	FVector2& operator-=(const FVector2& Other);
	FVector2& operator+=(float Scalar);
	FVector2& operator-=(float Scalar);
	FVector2& operator*=(float Scalar);
	FVector2& operator/=(float Scalar);
};