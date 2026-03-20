#pragma once

#include "../EngineAPI.h"

struct ENGINE_API FVector4
{
  public:
    constexpr FVector4(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f, float InW = 0.0f)
        : x(InX), y(InY), z(InZ), w(InW)
    {
    }
    ~FVector4() = default;

    static constexpr FVector4 Zero() { return FVector4(0.0f, 0.0f, 0.0f, 0.0f); }
    static constexpr FVector4 Up() { return FVector4(0.0f, 0.0f, 1.0f, 0.0f); }
    static constexpr FVector4 Right() { return FVector4(0.0f, 1.0f, 0.0f, 0.0f); }
    static constexpr FVector4 Forward() { return FVector4(1.0f, 0.0f, 0.0f, 0.0f); }
    static constexpr FVector4 Point() { return FVector4(0.0f, 0.0f, 0.0f, 1.0f); }

    float    Dot(const FVector4 &Other) const;
    FVector4 Cross(const FVector4 &Other) const;
    FVector4 operator+(const FVector4 &Other) const;
    FVector4 operator-(const FVector4 &Other) const;
    FVector4 operator*(const float s) const;
    FVector4 operator/(const float s) const;

    FVector4 Normalize() const;
    float    Length() const;

    bool IsNearlyEqual(const FVector4 &Other) const;
    bool operator==(const FVector4 &Other) const;

    bool IsPoint() const;
    bool IsVector() const;

  public:
    float x, y, z, w;
};
