#pragma once

#include "Vector.h"
#include "Utils.h"

struct FMatrix;

struct FVector4
{
public:
	//======================================//
	//				constructor				//
	//======================================//
	constexpr FVector4(const float InX = 0.0f, const float InY = 0.0f, const float InZ = 0.0f,
		const float InW = 0.0f) noexcept
		: X(InX), Y(InY), Z(InZ), W(InW)
	{
	}
	constexpr FVector4(const FVector& InVec, const float InW = 0.0f) noexcept
		: X(InVec.X), Y(InVec.Y), Z(InVec.Z), W(InW)
	{
	}
	~FVector4() = default;

	//======================================//
	//				static helper			//
	//======================================//
	static constexpr FVector4 ZeroVector() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
	static constexpr FVector4 ZeroPoint() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
	static constexpr FVector4 UpVector() { return { 0.0f, 0.0f, 1.0f, 0.0f }; }
	static constexpr FVector4 RightVector() { return { 0.0f, 1.0f, 0.0f, 0.0f }; }
	static constexpr FVector4 ForwardVector() { return { 1.0f, 0.0f, 0.0f, 0.0f }; }
	static constexpr FVector4 Zero() { return ZeroVector(); }
	static constexpr FVector4 Up() { return UpVector(); }
	static constexpr FVector4 Right() { return RightVector(); }
	static constexpr FVector4 Forward() { return ForwardVector(); }
	static constexpr FVector4 Point() { return ZeroPoint(); }
	static constexpr FVector4 Point(const float InX, const float InY, const float InZ)
	{
		return { InX, InY, InZ, 1.0f };
	}
	static constexpr FVector4 Vector(const float InX, const float InY, const float InZ)
	{
		return { InX, InY, InZ, 0.0f };
	}

	//======================================//
	//				operators				//
	//======================================//
	[[nodiscard]] float Dot(const FVector4& Other) const noexcept
	{
		const DirectX::XMVECTOR D = DirectX::XMVector3Dot(ToXMVector(), Other.ToXMVector());
		return DirectX::XMVectorGetX(D);

		// return X * Other.X + Y * Other.Y + Z * Other.Z;
	}

	[[nodiscard]] FVector4 Cross(const FVector4& Other) const noexcept
	{
		const DirectX::XMVECTOR C = DirectX::XMVector3Cross(ToXMVector(), Other.ToXMVector());
		DirectX::XMFLOAT3 T;
		DirectX::XMStoreFloat3(&T, C);
		return { T.x, T.y, T.z, 0.0f };

		// return {Y * Other.Z - Z * Other.Y, Z * Other.X - X * Other.Z, X * Other.Y - Y * Other.X};
	}

	FVector4 operator+(const FVector4& Other) const noexcept
	{
		// Homogeneous rules:
		// Point + Vector => Point
		// Vector + Point => Point
		// Vector + Vector => Vector
		// Point + Point => invalid
		const bool bThisPoint = IsPoint();
		const bool bOtherPoint = Other.IsPoint();
		assert(!(bThisPoint && bOtherPoint) && "FVector4: Point + Point is invalid.");

		const float ResultW = (bThisPoint || bOtherPoint) ? 1.0f : 0.0f;
		return { X + Other.X, Y + Other.Y, Z + Other.Z, ResultW };

		// return {X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W};
		// return {X + Other.X, Y + Other.Y, Z + Other.Z};
	}

	FVector4 operator-(const FVector4& Other) const noexcept
	{
		// Homogeneous rules:
		// Point - Point => Vector
		// Point - Vector => Point
		// Vector - Vector => Vector
		// Vector - Point => invalid
		const bool bThisPoint = IsPoint();
		const bool bOtherPoint = Other.IsPoint();
		assert((bThisPoint || !bOtherPoint) && "FVector4: Vector - Point is invalid.");

		const float ResultW = (bThisPoint && !bOtherPoint) ? 1.0f : 0.0f;
		return { X - Other.X, Y - Other.Y, Z - Other.Z, ResultW };

		// return {X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W};
		// return {X - Other.X, Y - Other.Y, Z - Other.Z};
	}

	FVector4 operator*(const float S) const noexcept
	{
		return { X * S, Y * S, Z * S, W * S };

		// return {X * S, Y * S, Z * S};
	}

	FVector4 operator/(const float S) const noexcept
	{
		if (std::abs(S) < MathUtil::Epsilon)
		{
			assert(S != 0.0f && "Division by zero in FVector4::operator/");
			return ZeroVector();
		}
		const float Denominator = 1.0f / S;
		return { X * Denominator, Y * Denominator, Z * Denominator, W * Denominator };

		// float Denominator = 1.0f / S;
		// return {X * Denominator, Y * Denominator, Z * Denominator};
	}

	[[nodiscard]] bool IsNearlyEqual(const FVector4& Other) const noexcept
	{
		return DirectX::XMVector4NearEqual(
			ToXMVector(),
			Other.ToXMVector(),
			DirectX::XMVectorReplicate(MathUtil::Epsilon));

		// return (std::abs(X - Other.X) < FMath::Epsilon) && (std::abs(Y - Other.Y) < FMath::Epsilon) &&
		//        (std::abs(Z - Other.Z) < FMath::Epsilon);
	}

	bool operator==(const FVector4& Other) const noexcept { return IsNearlyEqual(Other); }

	//======================================//
	//				  method				//
	//======================================//
	[[nodiscard]] FVector4 Normalize() const noexcept
	{
		const DirectX::XMVECTOR V = DirectX::XMVectorSet(X, Y, Z, 0.0f);
		const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(V));
		const float Denominator = std::sqrt(SquareSum);

		if (std::abs(Denominator) < MathUtil::Epsilon)
		{
			return ZeroVector();
		}

		const DirectX::XMVECTOR N = DirectX::XMVector3Normalize(V);
		DirectX::XMFLOAT3 T;
		DirectX::XMStoreFloat3(&T, N);
		return { T.x, T.y, T.z, 0.0f };

		// float SquareSum = X * X + Y * Y + Z * Z;
		// float Denominator = std::sqrt(SquareSum);
		//
		// if (std::abs(Denominator) < FMath::Epsilon)
		// {
		//     return ZeroVector();
		// }
		// Denominator = 1.0f / Denominator;
		//
		// return {X * Denominator, Y * Denominator, Z * Denominator};
	}

	[[nodiscard]] float Length() const noexcept
	{
		const DirectX::XMVECTOR V = DirectX::XMVectorSet(X, Y, Z, 0.0f);
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(V));

		// return std::sqrt(X * X + Y * Y + Z * Z);
	}

	[[nodiscard]] bool IsPoint(float Tolerance = 1.e-6f) const noexcept
	{
		return std::abs(W - 1.0f) <= Tolerance;
	}

	[[nodiscard]] bool IsVector(float Tolerance = 1.e-6f) const noexcept
	{
		return std::abs(W) <= Tolerance;
	}

	// Homogeneous -> 3D: point uses perspective divide, vector passes through.
	[[nodiscard]] FVector ToVector3(float Tolerance = 1.e-6f) const noexcept
	{
		if (std::abs(W) <= Tolerance)
		{
			return FVector(X, Y, Z);
		}

		const float InvW = 1.0f / W;
		return FVector(X * InvW, Y * InvW, Z * InvW);
	}

	XMVector ToXMVector() const noexcept { return DirectX::XMVectorSet(X, Y, Z, W); }

	FVector4 operator*(const FMatrix& Mat) const noexcept;

public:
	union
	{
		struct
		{
			float X;
			float Y;
			float Z;
			float W;
		};

		float XYZW[4];
	};
};
