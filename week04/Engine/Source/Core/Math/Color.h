#pragma once

#include "../EngineAPI.h"

struct ENGINE_API FColor
{
public:
	FColor(float InR = 0.f, float InG = 0.f, float InB = 0.f, float InA = 1.0f);
	~FColor();

	FColor operator+(float num) const;
	FColor operator+(const FColor& c) const;
	FColor operator-(float num) const;
	FColor operator-(const FColor& c) const;
	FColor operator*(float num) const;
	FColor operator*(const FColor& c) const;
	uint32 ToPackedABGR() const;

	static FColor Lerp(const FColor& A, const FColor& B, float T);

	static FColor White();
	static FColor Black();
	static FColor Red();
	static FColor Green();
	static FColor Blue();
	static FColor Yellow();
	static FColor Magenta();
	static FColor Cyan();
	static FColor Transparent();

public:
	float r, g, b, a;
};
