/*
    DecalApply.hlsli는 decal 결과를 scene surface에 합성하는 helper를 제공합니다.

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

#ifndef DECAL_APPLY_HLSLI
#define DECAL_APPLY_HLSLI

float4 ApplyDecalBaseColor(float4 BaseColor, float4 DecalColor, float Alpha)
{
    float3 Tinted = BaseColor.rgb * lerp(float3(1.0f, 1.0f, 1.0f), DecalColor.rgb, Alpha);
    return float4(Tinted, Alpha);
}

float3 ApplyDecalNormal(float3 BaseNormal, float4 DecalColor, float Alpha)
{
    float3 DetailNormal = normalize(float3(DecalColor.rg * 2.0f - 1.0f, 1.0f));
    return normalize(lerp(BaseNormal, DetailNormal, Alpha * 0.2f));
}

float4 ApplyDecalMaterialParam(float4 BaseParam, float4 DecalColor, float Alpha)
{
    float4 Result = BaseParam;
    Result.y = saturate(BaseParam.y + DecalColor.a * Alpha * 0.25f);
    Result.a = Alpha;
    return Result;
}

#endif
