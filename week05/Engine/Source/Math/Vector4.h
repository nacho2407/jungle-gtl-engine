#pragma once

#include "EngineAPI.h"
#include <cmath>
struct ENGINE_API FVector4
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;

    FVector4() = default;
    FVector4(float InX, float InY, float InZ, float InW)
        : X(InX), Y(InY), Z(InZ), W(InW) {}
	float Dot(const FVector4& other)
	{
		return {X*other.X+Y*other.Y+ Z * other.Z + W * other.W };
	}
	float Length() 
	{
		return std::sqrt(X * X + Y * Y + Z * Z);
	}
	float Length3() 
	{
		return std::sqrt(X * X + Y * Y + Z * Z+W*W);
	}
};
