#include "Math/FVector4.h"

float FVector4::Dot(const FVector4& other)
{
	return (x * other.x + y * other.y + z * other.z);
}

float FVector4::Length()
{
	return sqrt(x * x + y * y + z * z);
}

float FVector4::Length3()
{
	return sqrt(x * x + y * y + z * z + w * w);
}