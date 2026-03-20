#pragma once

#include "../EngineAPI.h"
#include "Vector.h"

struct ENGINE_API FMatrix
{
  public:
    constexpr FMatrix();
    ~FMatrix();

  public:
    static FMatrix IdentityMatrix();
    static FMatrix ZeroMatrix();

    static FMatrix TranslationMatrix(const FVector &InVec);
    static FMatrix TranslationMatrix(float InX = 0.f, float InY = 0.f, float InZ = 0.f);

    static FMatrix ScaleMatrix(const FVector &InVec);
    static FMatrix ScaleMatrix(float InX = 1.f, float InY = 1.f, float InZ = 1.f);

    FVector TransformPoint(const FVector &Vector) const;
    FVector TransformVector(const FVector &Vector) const;

    // Euler Angle
    static FMatrix RotationXMatrix(float InRoll);
    static FMatrix RotationYMatrix(float InPitch);
    static FMatrix RotationZMatrix(float InYaw);
    static FMatrix EulerRotationMatrix(const FRotator &InRot);
    static FMatrix EulerRotationMatrix(float InRoll = 0.0f, float InPitch = 0.0f,
                                       float InYaw = 0.0f);


    FMatrix operator*(const FMatrix &Other) const;

    static FMatrix ViewMatrix(FVector Eye, FVector Target, FVector Up);
    static FMatrix PerspectiveMatrix(float FovDegree, float Aspect, float Near, float Far);
    FMatrix Inverse() const;

    float *operator[](int Row) { return m[Row]; }
    const float *operator[](int Row) const { return m[Row]; }

  public:
    float m[4][4];
};
