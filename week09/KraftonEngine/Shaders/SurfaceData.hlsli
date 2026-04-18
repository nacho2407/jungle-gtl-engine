#ifndef SURFACE_DATA_HLSLI
#define SURFACE_DATA_HLSLI

float4 EncodeBaseColor(float4 Color)
{
    return Color;
}

float4 EncodeSurface1(float4 Value)
{
    return Value;
}

float4 EncodeMaterialParam(float4 Value)
{
    return Value;
}

float4 EncodeNormal(float3 Normal)
{
    return float4(normalize(Normal) * 0.5f + 0.5f, 1.0f);
}

float3 DecodeNormal(float4 EncodedNormal)
{
    return normalize(EncodedNormal.xyz * 2.0f - 1.0f);
}

float4 ResolveSurfaceValue(float4 BaseValue, float4 ModifiedValue)
{
    return (ModifiedValue.a > 0.0001f) ? ModifiedValue : BaseValue;
}

#endif
