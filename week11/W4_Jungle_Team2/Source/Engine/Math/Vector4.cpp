#include <cassert>
#include "Matrix.h"
#include "Core/CoreMinimal.h"
#include "Utils.h"

float FVector4::Dot(const FVector4& Other) const { return X * Other.X + Y * Other.Y + Z * Other.Z; }

FVector4 FVector4::Cross(const FVector4& Other) const
{
    return {Y * Other.Z - Z * Other.Y, Z * Other.X - X * Other.Z, X * Other.Y - Y * Other.X};
}

FVector4 FVector4::operator+(const FVector4& Other) const
{
    return {X + Other.X, Y + Other.Y, Z + Other.Z};
}

FVector4 FVector4::operator-(const FVector4& Other) const
{
    return {X - Other.X, Y - Other.Y, Z - Other.Z};
}

FVector4 FVector4::operator*(const float S) const { return {X * S, Y * S, Z * S}; }

FVector4 FVector4::operator/(const float S) const
{
    if (std::abs(S) < MathUtil::Epsilon)
    {
        assert(S != 0.0f && "Division by zero in FVector4::operator/");
        return Zero();
    }
    float Denominator = 1.0f / S;
    return {X * Denominator, Y * Denominator, Z * Denominator};
}

FVector4 FVector4::Normalize() const
{
    float SquareSum = X * X + Y * Y + Z * Z;
    float Denominator = std::sqrt(SquareSum);

    if (std::abs(Denominator) < MathUtil::Epsilon)
    {
        return Zero();
    }
    Denominator = 1.0f / Denominator;

    return {X * Denominator, Y * Denominator, Z * Denominator};
}

float FVector4::Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }

bool FVector4::IsNearlyEqual(const FVector4& Other) const
{
    return (std::abs(X - Other.X) < MathUtil::Epsilon) && (std::abs(Y - Other.Y) < MathUtil::Epsilon) &&
           (std::abs(Z - Other.Z) < MathUtil::Epsilon);
}

bool FVector4::operator==(const FVector4& Other) const { return IsNearlyEqual(Other); }

bool FVector4::IsPoint() const { return std::abs(W - 1) < MathUtil::Epsilon; }

bool FVector4::IsVector() const { return std::abs(W) < MathUtil::Epsilon; }

FVector4 FVector4::operator*(const FMatrix& Mat) const
{
    FVector4 NewVec4;

    for (int32 Col = 0; Col < 4; Col++)
    {
        
        NewVec4.XYZW[Col] =
            X * Mat.M[0][Col] + Y * Mat.M[1][Col] +
                            Z * Mat.M[2][Col] * W * Mat.M[3][Col];
    }
    return NewVec4;
}