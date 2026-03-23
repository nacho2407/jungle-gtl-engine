#include "../CoreMinimal.h"
#include <cassert>

#include "MathUtility.h"

FVector4 FVector4::Normalize() const
{
    float SquareSum = X * X + Y * Y + Z * Z;
    float Denominator = std::sqrt(SquareSum);

    if (std::abs(Denominator) < FMath::Epsilon)
    {
        return Zero();
    }
    Denominator = 1.0f / Denominator;

    return { X * Denominator, Y * Denominator, Z * Denominator };
}

float FVector4::Length() const
{
	return std::sqrt(X * X + Y * Y + Z * Z);
}

bool FVector4::IsNearlyEqual(const FVector4 &Other) const
{
    return (std::abs(X - Other.X) < FMath::Epsilon) &&
           (std::abs(Y - Other.Y) < FMath::Epsilon) &&
           (std::abs(Z - Other.Z) < FMath::Epsilon);
}

bool FVector4::operator==(const FVector4 &Other) const { return IsNearlyEqual(Other); }

bool FVector4::IsPoint() const { return std::abs(W - 1) < FMath::Epsilon; }

bool FVector4::IsVector() const { return std::abs(W) < FMath::Epsilon; }
