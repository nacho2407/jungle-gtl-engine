#pragma once

#include "Serializable.h"
#include "Matrix.h"
#include "Vector.h"

struct FRotator : public ISerializable
{
public:
    float Roll;  // X축 회전
    float Pitch; // Y축 회전
    float Yaw;  // Z축 회전
    
    static const FRotator ZeroRotator;

    FRotator() : Roll(0.0f), Pitch(0.0f), Yaw(0.0f)  {}
    FRotator(float InRoll, float InPitch, float InYaw)
        : Roll(InRoll), Pitch(InPitch), Yaw(InYaw) {
    }

    static FRotator FromVector(const FVector& Forward);
    FMatrix ToMatrix() const;
    void Normalize();

    FRotator operator+(const FRotator& Other) const;
    FRotator operator-(const FRotator& Other) const;
    FRotator& operator+=(const FRotator& Other);

    json::JSON Serialize();
    void Deserialize(json::JSON);
};
