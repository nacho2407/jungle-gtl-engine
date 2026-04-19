#include "CommonTypes.hlsli"   // Functions.hlsl → ConstantBuffers.hlsl (b0/b1/b4, FLocalLightInfo)
#include "SurfaceData.hlsli"
#include "LightingCommon.hlsli" // t6 StructuredBuffer<FLocalLightInfo>, 조명 함수들

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

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float4 BaseColor = ResolveBaseColor(UV);

#if defined(LIGHTING_MODEL_GOURAUD)
    // Gouraud는 Surface1에 인코딩되지 않은 라이팅 값이 들어있으므로 직접 사용
    float4 GouraudL = ResolveSurface1(UV);
    return ComputeGouraudLighting(BaseColor, GouraudL);
#elif defined(LIGHTING_MODEL_LAMBERT)
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    return ComputeLambertLighting(BaseColor, Normal);
#elif defined(LIGHTING_MODEL_PHONG)
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    return ComputeBlinnPhongLighting(BaseColor, Normal, MaterialParam, UV);
#else
    return BaseColor;
#endif
}
