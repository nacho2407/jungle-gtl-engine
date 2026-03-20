#pragma once

#include "../EngineAPI.h"

struct FMatrix;
struct FVector;
struct FRotator;

struct ENGINE_API FQuat
{
    constexpr FQuat(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f, float InW = 0.0f)
        : x(InX), y(InY), z(InZ), w(InW)
    {
    }
    ~FQuat() = default;

	static constexpr FQuat Zero() { return FQuat(); }

    static FQuat FromAxisAngle(const FVector &axis, float angleRadians);
    static FQuat FromEuler(const FRotator &eulerDegrees);
    FRotator     ToEuler() const;

    FQuat   operator*(const FQuat &q) const;
    FQuat   Conjugate() const;
    void    Normalize();
    FVector RotateVector(const FVector &v) const;
    FMatrix ToMatrix() const;

  public:
    float x, y, z, w;
};
