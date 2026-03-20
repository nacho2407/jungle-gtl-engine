#pragma once

#include "../EngineAPI.h"

struct FMatrix;

struct ENGINE_API FVector
{
  public:
    constexpr FVector(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f) : x(InX), y(InY), z(InZ)
    {
    }
    ~FVector() = default;

    constexpr static FVector Zero() { return FVector(0.0f, 0.0f, 0.0f); }
    constexpr static FVector Up() { return FVector(0.0f, 0.0f, 1.0f); }
    constexpr static FVector Right() { return FVector(0.0f, 1.0f, 0.0f); }
    constexpr static FVector Forward() { return FVector(1.0f, 0.0f, 0.0f); }

    float   Dot(const FVector &Other) const;
    FVector Cross(const FVector &Other) const;
    FVector operator+(const FVector &Other) const;
    FVector operator-(const FVector &Other) const;
    FVector operator*(const float s) const;
    FVector operator/(const float s) const;

    FVector Normalize() const;
    float   Length() const;

    bool IsNearlyEqual(const FVector &Other) const;
    bool operator==(const FVector &Other) const;

  public:
    float x, y, z;
};
