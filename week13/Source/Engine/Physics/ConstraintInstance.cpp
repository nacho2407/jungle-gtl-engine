#include "ConstraintInstance.h"

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    float DegreesToRadians(float Degrees)
    {
        return Degrees * Pi / 180.0f;
    }
}

float FConstraintInstance::GetTwistLimitRadians() const
{
    return DegreesToRadians(TwistLimitDegrees);
}

float FConstraintInstance::GetSwing1LimitRadians() const
{
    return DegreesToRadians(Swing1LimitDegrees);
}

float FConstraintInstance::GetSwing2LimitRadians() const
{
    return DegreesToRadians(Swing2LimitDegrees);
}
