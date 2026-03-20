#include "../CoreMinimal.h"
#include <cassert>

float FVector4::Dot(const FVector4 &Other) const { return x * Other.x + y * Other.y + z * Other.z; }

FVector4 FVector4::Cross(const FVector4 &Other) const
{
    return FVector4(y * Other.z - z * Other.y, z * Other.x - x * Other.z,
                    x * Other.y - y * Other.x);
}

FVector4 FVector4::operator+(const FVector4 &Other) const
{
    return FVector4(x + Other.x, y + Other.y, z + Other.z);
}

FVector4 FVector4::operator-(const FVector4 &Other) const
{
    return FVector4(x - Other.x, y - Other.y, z - Other.z);
}

FVector4 FVector4::operator*(const float s) const { return FVector4(x * s, y * s, z * s); }

FVector4 FVector4::operator/(const float s) const
{
    if (std::abs(s) < MathHelper::Epsilon)
    {
        assert(s != 0.0f && "Division by zero in FVector4::operator/");
        return FVector4::Zero();
    }
    float Denominator = 1.0f / s;
    return FVector4(x * Denominator, y * Denominator, z * Denominator);
}

FVector4 FVector4::Normalize() const
{
    float SquareSum = x * x + y * y + z * z;
    float Denominator = MathHelper::Sqrt(SquareSum);

    if (std::abs(Denominator) < MathHelper::Epsilon)
        return FVector4::Zero();
    Denominator = 1.0f / Denominator;

    return FVector4(x * Denominator, y * Denominator, z * Denominator);
}

float FVector4::Length() const { return MathHelper::Sqrt(x * x + y * y + z * z); }

bool FVector4::IsNearlyEqual(const FVector4 &Other) const
{
    return (std::abs(x - Other.x) < MathHelper::Epsilon) &&
           (std::abs(y - Other.y) < MathHelper::Epsilon) &&
           (std::abs(z - Other.z) < MathHelper::Epsilon);
}

bool FVector4::operator==(const FVector4 &Other) const { return IsNearlyEqual(Other); }

bool FVector4::IsPoint() const { return std::abs(w - 1) < MathHelper::Epsilon; }

bool FVector4::IsVector() const { return std::abs(w) < MathHelper::Epsilon; }
