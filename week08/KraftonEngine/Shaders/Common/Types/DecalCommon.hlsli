#ifndef DECAL_COMMON_HLSLI
#define DECAL_COMMON_HLSLI

float3 ReconstructPositionFromDepth(float2 UV, float Depth, float4x4 InverseViewProjection)
{
    float4 Clip = float4(UV * 2.0f - 1.0f, Depth, 1.0f);
    Clip.y *= -1.0f;
    float4 World = mul(Clip, InverseViewProjection);
    return World.xyz / max(World.w, 0.0001f);
}

bool IsInsideDecalBounds(float3 LocalPosition)
{
    return abs(LocalPosition.x) <= 0.5f
        && abs(LocalPosition.y) <= 0.5f
        && abs(LocalPosition.z) <= 0.5f;
}

float2 ProjectDecalUV(float3 LocalPosition)
{
    return float2(LocalPosition.y + 0.5f, 0.5f - LocalPosition.z);
}

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
