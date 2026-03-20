#pragma once

#include <cmath>

class MathHelper
{
  public:
    static constexpr float PI{3.141592654f};
    static constexpr float Epsilon{1e-6f};

    template <typename T> static inline T Clamp(const T Value, const T Min, const T Max)
    {
        return (Value < Min) ? Min : (Value > Max) ? Max : Value;
    }

    static inline float Lerp(float a, float b, float t) { return (1.0f - t) * a + t * b; }

    static inline float DegToRad(float degree) { return degree * (PI / 180.0f); }

    static inline float RadToDeg(float radian) { return radian * (180.0f / PI); }

    static inline float Atan2(float y, float x) { return std::atan2(y, x); }

    static inline float NormalizeAngle(float Angle)
    {
        Angle = std::fmod(Angle, 360.0f);
        if (Angle > 180.0f)
            Angle -= 360.0f;
        if (Angle <= -180.0f)
            Angle += 360.0f;
        return Angle;
    }

    static inline float Sqrt(float Value) { return std::sqrt(Value); }

  private:
    MathHelper();
    ~MathHelper();
};
