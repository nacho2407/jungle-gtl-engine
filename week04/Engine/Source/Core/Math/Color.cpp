#include "../CoreMinimal.h"

constexpr FColor::FColor(float InR, float InG, float InB, float InA)
	: r(InR), g(InG), b(InB), a(InA) {
}

FColor::~FColor()
{

}

static float ClampColor(float Value) {
	return MathHelper::Clamp(Value, 0.0f, 1.0f);
}

FColor FColor::operator+(float num) const {
	return {
		ClampColor(r + num),
		ClampColor(g + num),
		ClampColor(b + num),
		a
	};
}

FColor FColor::operator+(const FColor& c) const {
	return {
		ClampColor(r + c.r),
		ClampColor(g + c.g),
		ClampColor(b + c.b),
		ClampColor(a + c.a)
	};
}

FColor FColor::operator-(float num) const {
	return {
		ClampColor(r - num),
		ClampColor(g - num),
		ClampColor(b - num),
		a
	};
}

FColor FColor::operator-(const FColor& c) const {
	return {
		ClampColor(r - c.r),
		ClampColor(g - c.g),
		ClampColor(b - c.b),
		ClampColor(a - c.a)
	};
}

FColor FColor::operator*(float num) const {
	return {
		ClampColor(r * num),
		ClampColor(g * num),
		ClampColor(b * num),
		a
	};
}

FColor FColor::operator*(const FColor& c) const {
	return {
		ClampColor(r * c.r),
		ClampColor(g * c.g),
		ClampColor(b * c.b),
		ClampColor(a * c.a)
	};
}

uint32 FColor::ToPackedABGR() const {
	uint32 R = static_cast<uint32>(r * 255.999f);
	uint32 G = static_cast<uint32>(g * 255.999f);
	uint32 B = static_cast<uint32>(b * 255.999f);
	uint32 A = static_cast<uint32>(a * 255.999f);
	return (A << 24) | (B << 16) | (G << 8) | R;
}

FColor FColor::Lerp(const FColor& A, const FColor& B, float T) {
	return {
		ClampColor(A.r + (B.r - A.r) * T),
		ClampColor(A.g + (B.g - A.g) * T),
		ClampColor(A.b + (B.b - A.b) * T),
		ClampColor(A.a + (B.a - A.a) * T)
	};
}

FColor FColor::White()
{
	return FColor(1.0f, 1.0f, 1.0f, 1.0f);
}

FColor FColor::Black()
{
	return FColor(0.0f, 0.0f, 0.0f, 1.0f);
}

FColor FColor::Red()
{
	return FColor(1.0f, 0.0f, 0.0f, 1.0f);
}

FColor FColor::Green()
{
	return FColor(0.0f, 1.0f, 0.0f, 1.0f);
}

FColor FColor::Blue()
{
	return FColor(0.0f, 0.0f, 1.0f, 1.0f);
}

FColor FColor::Yellow()
{
	return FColor(1.0f, 1.0f, 0.0f, 1.0f);
}

FColor FColor::Magenta()
{
	return FColor(1.0f, 0.0f, 1.0f, 1.0f);
}

FColor FColor::Cyan()
{
	return FColor(0.0f, 1.0f, 1.0f, 1.0f);
}

FColor FColor::Transparent()
{
	return FColor(0.0f, 0.0f, 0.0f, 0.0f);
}