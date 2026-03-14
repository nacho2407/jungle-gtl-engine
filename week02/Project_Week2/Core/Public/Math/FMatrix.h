#pragma once

struct FMatrix
{
	float M[4][4];
	static const FMatrix Identity;

	FMatrix operator*(const FMatrix& rhs) const;
};