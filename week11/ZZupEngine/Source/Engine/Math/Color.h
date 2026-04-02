#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector4.h"

struct FColor
{
public:
	union
	{
		struct
		{
			float r, g, b, a;
		};
		struct
		{
			float R, G, B, A;
		};
	};

	constexpr FColor() noexcept : r(0.f), g(0.f), b(0.f), a(1.f) {}

	constexpr FColor(float InR, float InG, float InB, float InA) : r(InR), g(InG), b(InB), a(InA) {}
	constexpr FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA = 255)
		: r(static_cast<float>(InR) / 255.0f)
		, g(static_cast<float>(InG) / 255.0f)
		, b(static_cast<float>(InB) / 255.0f)
		, a(static_cast<float>(InA) / 255.0f)
	{
	}
	~FColor() = default;

public:
	static constexpr FColor White() { return FColor(1.0f, 1.0f, 1.0f, 1.0f); }

	static constexpr FColor Black() { return FColor(0.0f, 0.0f, 0.0f, 1.0f); }

	static constexpr FColor Red() { return FColor(1.0f, 0.0f, 0.0f, 1.0f); }

	static constexpr FColor Green() { return FColor(0.0f, 1.0f, 0.0f, 1.0f); }

	static constexpr FColor Blue() { return FColor(0.0f, 0.0f, 1.0f, 1.0f); }

	static constexpr FColor Yellow() { return FColor(1.0f, 1.0f, 0.0f, 1.0f); }

	static constexpr FColor Magenta() { return FColor(1.0f, 0.0f, 1.0f, 1.0f); }

	static constexpr FColor Cyan() { return FColor(0.0f, 1.0f, 1.0f, 1.0f); }

	static constexpr FColor Gray() { return FColor(0.545f, 0.545f, 0.545f, 1.0f); }

	static constexpr FColor Transparent() { return FColor(0.0f, 0.0f, 0.0f, 0.0f); }

	FColor operator+(float Num) const;
	FColor operator+(const FColor& Other) const;
	FColor operator-(float Num) const;
	FColor operator-(const FColor& Other) const;
	FColor operator*(float Num) const;
	FColor operator*(const FColor& Other) const;
	FVector4 ToVector4() const { return FVector4(r, g, b, a); }
	uint32 ToPackedABGR() const;

	static FColor Lerp(const FColor& A, const FColor& B, float T);

};
