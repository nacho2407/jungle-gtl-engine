/*
    BRDF.hlsli는 scene 셰이더에서 사용하는 기본 BRDF helper를 제공합니다.

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

#ifndef BRDF_HLSLI
#define BRDF_HLSLI

float ComputeLambertDiffuse(float3 Normal, float3 LightDirection)
{
    return saturate(dot(normalize(Normal), normalize(LightDirection)));
}

float ComputeBlinnPhongSpecular(float3 Normal, float3 LightDirection, float3 ViewDirection, float Shininess)
{
    float3 H = normalize(normalize(ViewDirection) + normalize(LightDirection));
    return pow(saturate(dot(normalize(Normal), H)), max(Shininess, 1.0f));
}

float4 ComputeUnlitLighting(float4 BaseColor)
{
    return BaseColor;
}

float4 ComputeGouraudLitColor(float4 BaseColor, float4 GouraudLighting)
{
    return float4(BaseColor.rgb * GouraudLighting.rgb, BaseColor.a);
}

#endif
