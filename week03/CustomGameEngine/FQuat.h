#pragma once

struct FMatrix;
class FVector;

struct FQuat {
    float x, y, z, w;

    FQuat() : x(0), y(0), z(0), w(1) {} // 항등 쿼터니언 (회전 없음)
    FQuat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    static FQuat FromAxisAngle(const FVector& axis, float angleRadians);

    static FQuat FromEuler(const FVector& eulerDegrees);
    FVector ToEuler() const;

    FQuat operator*(const FQuat& q) const;
    FQuat Conjugate() const;
    void Normalize();
    FVector RotateVector(const FVector& v) const;
    FMatrix ToMatrix() const;
};
