#pragma once
#include "Math/MathHelper.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"

struct FRotator
{
public:
    float Pitch; // Y축 회전
    float Yaw;   // Z축 회전
    float Roll;  // X축 회전

    static const FRotator ZeroRotator;

    FRotator() : Pitch(0.0f), Yaw(0.0f), Roll(0.0f) {}
    FRotator(float InPitch, float InYaw, float InRoll)
        : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {
    }

    static FRotator FromVector(const FVector& Forward);
    FMatrix ToMatrix() const;
    void Normalize();

    FRotator operator+(const FRotator& Other) const;
    FRotator operator-(const FRotator& Other) const;
    FRotator& operator+=(const FRotator& Other);
};
