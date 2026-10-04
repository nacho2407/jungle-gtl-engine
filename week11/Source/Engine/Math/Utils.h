#pragma once

namespace MathUtil
{
    static constexpr float Epsilon{1e-6f};

    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float InvPI = 0.31830988618379067154f;
    static constexpr float HalfPI = 1.57079632679489661923f;
    static constexpr float TwoPi = 6.28318530717958647692f;
    static constexpr float DEG_TO_RAD = PI / 180;
    static constexpr float RAD_TO_DEG = 180 / PI;

    static constexpr float SmallNumber = 1.e-8f;
    static constexpr float KindaSmallNumber = 1.e-4f;

    static constexpr float DegreesToRadians(float Degrees) { return Degrees * (PI / 180.0f); }

    static constexpr float RadiansToDegrees(float Radians) { return Radians * (180.0f / PI); }

    static constexpr float Abs(float Value) { return (Value < 0.0f) ? -Value : Value; }

    static constexpr bool IsNearlyZero(float Value, float Tolerance = Epsilon) { return Abs(Value) <= Tolerance; }

    static constexpr bool IsNearlyEqual(float A, float B, float Tolerance = Epsilon) { return Abs(A - B) <= Tolerance; }

    template <typename T> static inline T Clamp(const T Value, const T Min, const T Max)
    {
        return (Value < Min) ? Min : (Value > Max) ? Max : Value;
    }
} // namespace MathUtil
