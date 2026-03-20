#pragma once

#include "../EngineAPI.h"

struct ENGINE_API FColor
{
  public:
    constexpr FColor(float InR, float InG, float InB, float InA) : r(InR), g(InG), b(InB), a(InA) {}
    ~FColor() = default;

  public:
    static constexpr FColor White() { return FColor(1.0f, 1.0f, 1.0f, 1.0f); }

    static constexpr FColor Black() { return FColor(0.0f, 0.0f, 0.0f, 1.0f); }

    static constexpr FColor Red() { return FColor(1.0f, 0.0f, 0.0f, 1.0f); }

    static constexpr FColor Green() { return FColor(0.0f, 1.0f, 0.0f, 1.0f); }

    static constexpr FColor Blue() { return FColor(0.0f, 0.0f, 1.0f, 1.0f); }

    static constexpr FColor Yellow() { return FColor(1.0f, 1.0f, 0.0f, 1.0f); }

    static constexpr FColor Magenta() { return FColor(1.0f, 0.0f, 1.0f, 1.0f); }

    static constexpr FColor Cyan() { return FColor(0.0f, 1.0f, 1.0f, 1.0f); }

    static constexpr FColor Transparent() { return FColor(0.0f, 0.0f, 0.0f, 0.0f); }

    FColor operator+(float Num) const;
    FColor operator+(const FColor &Other) const;
    FColor operator-(float Num) const;
    FColor operator-(const FColor &Other) const;
    FColor operator*(float Num) const;
    FColor operator*(const FColor &Other) const;
    uint32 ToPackedABGR() const;

    static FColor Lerp(const FColor &A, const FColor &B, float T);

  public:
    float r, g, b, a;
};
