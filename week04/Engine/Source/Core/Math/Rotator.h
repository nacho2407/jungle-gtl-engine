#pragma once

#include "../EngineAPI.h"

struct FMatrix;

struct ENGINE_API FRotator
{
  public:
    constexpr FRotator(float InRoll = 0.0f, float InPitch = 0.0f, float InYaw = 0.0f)
        : Roll(InRoll), Pitch(InPitch), Yaw(InYaw)
    {
    }
    ~FRotator() = default;

    constexpr FRotator Zero() { return FRotator(); }

    FMatrix ToMatrix() const;
    void    Normalize();

    FRotator  operator+(const FRotator &Other) const;
    FRotator  operator-(const FRotator &Other) const;
    FRotator &operator+=(const FRotator &Other);
    FRotator &operator-=(const FRotator &Other);

    static FRotator FromVector(const FVector &Forward);

  public:
    float Roll;  // X축 회전
    float Pitch; // Y축 회전
    float Yaw;   // Z축 회전
};
