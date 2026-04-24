
/*
    SurfaceData.hlsli는 셰이딩 표면 데이터 타입을 정의합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

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
    return float4(Value.x / 256.0f, Value.y, Value.z, Value.w);
}

float4 DecodeMaterialParam(float4 EncodedValue)
{
    return float4(EncodedValue.x * 256.0f, EncodedValue.y, EncodedValue.z, EncodedValue.w);
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

