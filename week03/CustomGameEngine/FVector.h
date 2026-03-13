#pragma once

// Structure for a 3D vector
class FVector
{
public:
    FVector(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
    float Dot(const FVector& Other) const;
    FVector operator+(const FVector& Other) const;
    FVector operator-(const FVector& Other) const;
    FVector operator*(const float s) const;
    FVector operator/(const float s) const;

    FVector Normalize() const;
    float Length() const;

public:
    float x, y, z;
};

float Dot(const FVector& a, const FVector& b);
FVector Add(const FVector& a, const FVector& b);
FVector Sub(const FVector& a, const FVector& b);
FVector Mul(const FVector& v, float s);
FVector Div(const FVector& v, float s);