#include "../CoreMinimal.h"

FQuat FQuat::FromAxisAngle(const FVector &axis, float angleRadians)
{
    float halfAngle = angleRadians * 0.5f;
    float s = std::sinf(halfAngle);
    return FQuat(axis.x * s, axis.y * s, axis.z * s, std::cosf(halfAngle));
}

FQuat FQuat::FromEuler(const FRotator &eulerDegrees)
{
    FQuat qRoll = FromAxisAngle(FVector(1, 0, 0), MathHelper::DegToRad(eulerDegrees.Roll));
    FQuat qPitch = FromAxisAngle(FVector(0, 1, 0), MathHelper::DegToRad(eulerDegrees.Pitch));
    FQuat qYaw = FromAxisAngle(FVector(0, 0, 1), MathHelper::DegToRad(eulerDegrees.Yaw));

    return qYaw * qPitch * qRoll;
}

FRotator FQuat::ToEuler() const
{
    float xsqr = x * x;
    float ysqr = y * y;
    float zsqr = z * z;

    float t0 = +2.0f * (w * x + y * z);
    float t1 = +1.0f - 2.0f * (xsqr + ysqr);
    float roll = std::atan2f(t0, t1);

    float t2 = +2.0f * (w * y - z * x);
    t2 = MathHelper::Clamp(t2, -1.0f, 1.0f);
    float pitch = std::asinf(t2);

    float t3 = +2.0f * (w * z + x * y);
    float t4 = +1.0f - 2.0f * (ysqr + zsqr);
    float yaw = std::atan2f(t3, t4);

    return FRotator(MathHelper::RadToDeg(roll), MathHelper::RadToDeg(pitch),
                    MathHelper::RadToDeg(yaw));
}

FQuat FQuat::operator*(const FQuat &q) const
{
    return FQuat(w * q.x + x * q.w + y * q.z - z * q.y, w * q.y + y * q.w + z * q.x - x * q.z,
                 w * q.z + z * q.w + x * q.y - y * q.x, w * q.w - x * q.x - y * q.y - z * q.z);
}

FQuat FQuat::Conjugate() const { return FQuat(-x, -y, -z, w); }

void FQuat::Normalize()
{
    float magSq = x * x + y * y + z * z + w * w;
    if (magSq > 0.0001f)
    {
        float invMag = 1.0f / sqrtf(magSq);
        x *= invMag;
        y *= invMag;
        z *= invMag;
        w *= invMag;
    }
}

FVector FQuat::RotateVector(const FVector &v) const
{
    // 최적화된 v' = v + 2w(q_v x v) + 2(q_v x (q_v x v))
    FVector qv(x, y, z);
    FVector t = qv.Cross(v) * 2.0f;
    return v + (t * w) + qv.Cross(t);
}

FMatrix FQuat::ToMatrix() const
{
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    FMatrix m; // 항등 행렬로 초기화되었다고 가정
    m[0][0] = 1.0f - 2.0f * (yy + zz);
    m[0][1] = 2.0f * (xy + wz);
    m[0][2] = 2.0f * (xz - wy);

    m[1][0] = 2.0f * (xy - wz);
    m[1][1] = 1.0f - 2.0f * (xx + zz);
    m[1][2] = 2.0f * (yz + wx);

    m[2][0] = 2.0f * (xz + wy);
    m[2][1] = 2.0f * (yz - wx);
    m[2][2] = 1.0f - 2.0f * (xx + yy);
    return m;
}
