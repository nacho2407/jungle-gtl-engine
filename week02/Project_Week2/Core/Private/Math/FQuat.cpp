#include "Math/FQuat.h"

#include <cmath>
#include <algorithm>

FQuat::FQuat()
    : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f)
{
}

FQuat::FQuat(float InX, float InY, float InZ, float InW)
    : X(InX), Y(InY), Z(InZ), W(InW)
{
}

FQuat FQuat::Identity()
{
    return FQuat(0.0f, 0.0f, 0.0f, 1.0f);
}

FQuat FQuat::FromAxisAngle(const FVector& Axis, float Radians)
{
    FVector N = Axis.GetNormalized();
    const float Half = Radians * 0.5f;
    const float S = std::sin(Half);
    const float C = std::cos(Half);

    return FQuat(N.X * S, N.Y * S, N.Z * S, C);
}

FQuat FQuat::FromEulerXYZ(const FVector& EulerRadians)
{
    const FQuat Qx = FromAxisAngle(FVector(1.0f, 0.0f, 0.0f), EulerRadians.X);
    const FQuat Qy = FromAxisAngle(FVector(0.0f, 1.0f, 0.0f), EulerRadians.Y);
    const FQuat Qz = FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), EulerRadians.Z);

    FQuat Result = Qx * Qy * Qz;
    Result.Normalize();
    return Result;
}

FVector FQuat::ToEulerXYZ() const
{
    const FMatrix M = ToMatrix();

    const float SinY = M.M[0][2];
    const float Yaw = std::asin(std::clamp(SinY, -1.0f, 1.0f));

    float Pitch = 0.0f;
    float Roll = 0.0f;

    const float CosY = std::cos(Yaw);
    if (std::fabs(CosY) > 0.0001f)
    {
        Pitch = std::atan2(-M.M[1][2], M.M[2][2]);
        Roll = std::atan2(-M.M[0][1], M.M[0][0]);
    }
    else
    {
        Pitch = std::atan2(M.M[2][1], M.M[1][1]);
        Roll = 0.0f;
    }

    return FVector(Pitch, Yaw, Roll);
}

void FQuat::Normalize()
{
    const float LenSq = X * X + Y * Y + Z * Z + W * W;
    if (LenSq <= 0.000001f)
    {
        X = 0.0f;
        Y = 0.0f;
        Z = 0.0f;
        W = 1.0f;
        return;
    }

    const float InvLen = 1.0f / std::sqrt(LenSq);
    X *= InvLen;
    Y *= InvLen;
    Z *= InvLen;
    W *= InvLen;
}

FQuat FQuat::GetNormalized() const
{
    FQuat Copy = *this;
    Copy.Normalize();
    return Copy;
}

FQuat FQuat::Conjugate() const
{
    return FQuat(-X, -Y, -Z, W);
}

FMatrix FQuat::ToMatrix() const
{
    const FQuat Q = GetNormalized();

    const float XX = Q.X * Q.X;
    const float YY = Q.Y * Q.Y;
    const float ZZ = Q.Z * Q.Z;
    const float XY = Q.X * Q.Y;
    const float XZ = Q.X * Q.Z;
    const float YZ = Q.Y * Q.Z;
    const float WX = Q.W * Q.X;
    const float WY = Q.W * Q.Y;
    const float WZ = Q.W * Q.Z;

    FMatrix M = FMatrix::MakeIdentity();

    M.M[0][0] = 1.0f - 2.0f * (YY + ZZ);
    M.M[0][1] = 2.0f * (XY + WZ);
    M.M[0][2] = 2.0f * (XZ - WY);

    M.M[1][0] = 2.0f * (XY - WZ);
    M.M[1][1] = 1.0f - 2.0f * (XX + ZZ);
    M.M[1][2] = 2.0f * (YZ + WX);

    M.M[2][0] = 2.0f * (XZ + WY);
    M.M[2][1] = 2.0f * (YZ - WX);
    M.M[2][2] = 1.0f - 2.0f * (XX + YY);

    return M;
}

FQuat FQuat::operator*(const FQuat& Rhs) const
{
    return FQuat(
        W * Rhs.X + X * Rhs.W + Y * Rhs.Z - Z * Rhs.Y,
        W * Rhs.Y - X * Rhs.Z + Y * Rhs.W + Z * Rhs.X,
        W * Rhs.Z + X * Rhs.Y - Y * Rhs.X + Z * Rhs.W,
        W * Rhs.W - X * Rhs.X - Y * Rhs.Y - Z * Rhs.Z
    );
}