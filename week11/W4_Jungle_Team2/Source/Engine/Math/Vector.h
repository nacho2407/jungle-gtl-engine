#pragma once
#include <cmath>

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
	
	float Length() const;
	float LengthSquared() const;
	void Normalize();
	void NormalizeSafe(float Epsilon = 1e-6f);
	FVector Normalized() const;
	FVector GetSafeNormal(float Epsilon = 1e-6f) const;

	bool IsNearlyZero(float Epsilon = 1e-6f) const;
	bool Equals(const FVector& Other, float Epsilon = 1e-6f) const;
	void Set(float InX, float InY, float InZ);

	float DotProduct(const FVector& Other) const;
	FVector CrossProduct(const FVector& Other) const;
	float MaxComponent() const;
	float MinComponent() const;

	static FVector CrossProduct(const FVector& v1, const FVector& v2) { return v1.CrossProduct(v2); }
	static float DotProduct(const FVector& v1, const FVector& v2) { return v1.DotProduct(v2); }
	static float Distance(const FVector& V1, const FVector& V2);
	static float DistSquared(const FVector& V1, const FVector& V2);
	static FVector Lerp(const FVector& A, const FVector& B, float Alpha);
	static FVector Reflect(const FVector& Direction, const FVector& Normal);
	static FVector Project(const FVector& Vector, const FVector& OnNormal);
	static FVector ZeroVector();
	static FVector OneVector();
	static FVector UpVector();
	static FVector RightVector();
	static FVector ForwardVector();

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

	FVector4(const FVector& Other) {
		X = Other.X;
		Y = Other.Y;
		Z = Other.Z;
		W = 1.0f;
	}

	float Length() const;
	float LengthSquared() const;
	void Normalize();
	void NormalizeSafe(float Epsilon = 1e-6f);
	FVector4 Normalized() const;
	FVector4 GetSafeNormal(float Epsilon = 1e-6f) const;

	bool IsNearlyZero(float Epsilon = 1e-6f) const;
	bool Equals(const FVector4& Other, float Epsilon = 1e-6f) const;
	void Set(float InX, float InY, float InZ, float InW);

	float Dot(const FVector4& Other) const;
	FVector4 Cross(const FVector4& Other) const;
	static FVector4 Cross(const FVector4& v1, const FVector4& v2) { return v1.Cross(v2); }
	static FVector4 Lerp(const FVector4& A, const FVector4& B, float Alpha);

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
	float LengthSquared() const;
	void Normalize();
	void NormalizeSafe(float Epsilon = 1e-6f);
	FVector2 Normalized() const;
	FVector2 GetSafeNormal(float Epsilon = 1e-6f) const;

	bool IsNearlyZero(float Epsilon = 1e-6f) const;
	bool Equals(const FVector2& Other, float Epsilon = 1e-6f) const;
	void Set(float InX, float InY);

	float DotProduct(const FVector2& Other) const;

	static float Distance(const FVector2& V1, const FVector2& V2);
	static float DistSquared(const FVector2& V1, const FVector2& V2);
	static FVector2 Lerp(const FVector2& A, const FVector2& B, float Alpha);
	static FVector2 ZeroVector();
	static FVector2 OneVector();

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