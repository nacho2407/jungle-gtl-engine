#pragma once
#include <cmath>

struct FVector4
{
	float x;
	float y;
	float z;
	float w;

	float Dot(const FVector4& other);
	float Length();
	float Length3();
};