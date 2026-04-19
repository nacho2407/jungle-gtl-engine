#include "CommonTypes.hlsli"
#include "SurfaceData.hlsli"
#include "LightingCommon.hlsli"

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);
Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);

float4 ResolveBaseColor(float2 UV)
{
    float4 BaseColor = g_BaseColorTex.Sample(LinearClampSampler, UV);
    float4 ModifiedBaseColor = g_ModifiedBaseColorTex.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(BaseColor, ModifiedBaseColor);
}

float4 ResolveSurface1(float2 UV)
{
    float4 Surface1 = g_Surface1Tex.Sample(LinearClampSampler, UV);
    float4 ModifiedSurface1 = g_ModifiedSurface1Tex.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(Surface1, ModifiedSurface1);
}

float4 ResolveSurface2(float2 UV)
{
    float4 Surface2 = g_Surface2Tex.Sample(LinearClampSampler, UV);
    float4 ModifiedSurface2 = g_ModifiedSurface2Tex.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(Surface2, ModifiedSurface2);
}

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_Lighting_Unlit(PS_Input_UV Input) : SV_TARGET0
{
    return ResolveBaseColor(Input.uv);
}

float4 PS_Lighting_Gouraud(PS_Input_UV Input) : SV_TARGET0
{
    float4 BaseColor = ResolveBaseColor(Input.uv);
    float4 GouraudL = g_Surface1Tex.Sample(LinearClampSampler, Input.uv);
    return ComputeGouraudLighting(BaseColor, GouraudL);
}

float4 PS_Lighting_Lambert(PS_Input_UV Input) : SV_TARGET0
{
    float4 BaseColor = ResolveBaseColor(Input.uv);
    float3 Normal = DecodeNormal(ResolveSurface1(Input.uv));
    return ComputeLambertLighting(BaseColor, Normal);
}

float4 PS_Lighting_BlinnPhong(PS_Input_UV Input) : SV_TARGET0
{
    float4 BaseColor = ResolveBaseColor(Input.uv);
    float3 Normal = DecodeNormal(ResolveSurface1(Input.uv));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(Input.uv));
    return ComputeBlinnPhongLighting(BaseColor, Normal, MaterialParam, Input.uv);
}
