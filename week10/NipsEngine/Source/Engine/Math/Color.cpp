#include "Utils.h"
#include "Core/CoreMinimal.h"

static float ClampColor(float Value) { return MathUtil::Clamp(Value, 0.0f, 1.0f); }

FColor FColor::operator+(float Num) const
{
	return {ClampColor(r + Num), ClampColor(g + Num), ClampColor(b + Num), a};
}

FColor FColor::operator+(const FColor &Other) const
{
	return {ClampColor(r + Other.r), ClampColor(g + Other.g), ClampColor(b + Other.b), ClampColor(a + Other.a)};
}

FColor FColor::operator-(float Num) const
{
	return {ClampColor(r - Num), ClampColor(g - Num), ClampColor(b - Num), a};
}

FColor FColor::operator-(const FColor &Other) const
{
	return {ClampColor(r - Other.r), ClampColor(g - Other.g), ClampColor(b - Other.b), ClampColor(a - Other.a)};
}

FColor FColor::operator*(float Num) const
{
	return {ClampColor(r * Num), ClampColor(g * Num), ClampColor(b * Num), a};
}

FColor FColor::operator*(const FColor &Other) const
{
	return {ClampColor(r * Other.r), ClampColor(g * Other.g), ClampColor(b * Other.b), ClampColor(a * Other.a)};
}

uint32 FColor::ToPackedABGR() const
{
	uint32 R = static_cast<uint32>(r * 255.999f);
	uint32 G = static_cast<uint32>(g * 255.999f);
	uint32 B = static_cast<uint32>(b * 255.999f);
	uint32 A = static_cast<uint32>(a * 255.999f);
	return (A << 24) | (B << 16) | (G << 8) | R;
}

FColor FColor::Lerp(const FColor &A, const FColor &B, float T)
{
	return {ClampColor(A.r + (B.r - A.r) * T), ClampColor(A.g + (B.g - A.g) * T),
			ClampColor(A.b + (B.b - A.b) * T), ClampColor(A.a + (B.a - A.a) * T)};
}
