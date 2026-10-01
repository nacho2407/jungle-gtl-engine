#ifndef SHARED_MATH_HLSLI
#define SHARED_MATH_HLSLI

float SafeRcp(float Value)
{
    return (abs(Value) > 0.000001f) ? (1.0f / Value) : 0.0f;
}

#endif
