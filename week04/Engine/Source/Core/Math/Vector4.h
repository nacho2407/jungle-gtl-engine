#pragma once

#include "Core/EngineAPI.h"

struct ENGINE_API FVector4
{
  public:
    constexpr FVector4(const float InX = 0.0f, const float InY = 0.0f, const float InZ = 0.0f, const float InW = 0.0f)
        : X(InX), Y(InY), Z(InZ), W(InW)
    {
    }
    ~FVector4() = default;

    static constexpr FVector4 Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Up() { return {0.0f, 0.0f, 1.0f, 0.0f}; }
    static constexpr FVector4 Right() { return {0.0f, 1.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Forward() { return {1.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr FVector4 Point() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] float    Dot(const FVector4 &Other) const;
    [[nodiscard]] FVector4 Cross(const FVector4 &Other) const;
    FVector4 operator+(const FVector4 &Other) const;
    FVector4 operator-(const FVector4 &Other) const;
    FVector4 operator*(const float S) const;
    FVector4 operator/(const float S) const;

    [[nodiscard]] FVector4 Normalize() const;
    [[nodiscard]] float    Length() const;

    [[nodiscard]] bool IsNearlyEqual(const FVector4 &Other) const;
    bool operator==(const FVector4 &Other) const;

    [[nodiscard]] bool IsPoint() const;
    [[nodiscard]] bool IsVector() const;

    XMVector ToXMVector() const noexcept { return DirectX::XMVectorSet(X, Y, Z, W); }

  public:
    float X, Y, Z, W;
};
