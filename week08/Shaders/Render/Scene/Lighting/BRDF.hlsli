/*
    BRDF.hlsli
    Lambert diffuse, Blinn-Phong specular 같은 기본 BRDF 보조 함수를 모아둔 헤더입니다.
    직접적인 바인딩 없이 수학 함수만 제공하며, 실제 리소스 사용은 include하는 조명 셰이더가 담당합니다.
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
