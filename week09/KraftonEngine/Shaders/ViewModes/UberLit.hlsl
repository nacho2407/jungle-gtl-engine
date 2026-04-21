#include "../Common/Types/CommonTypes.hlsli"
#include "../Common/Types/SurfaceData.hlsli"
#include "../Common/Types/LightingCommon.hlsli"

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
    float4 FinalColor = BaseColor;

#if defined(LIGHTING_MODEL_GOURAUD)
    // Gouraud Shading: Per-vertex lighting color is stored in Surface1
    float4 GouraudLighting = ResolveSurface1(UV);
    FinalColor = ComputeGouraudLighting(BaseColor, GouraudLighting);

#elif defined(LIGHTING_MODEL_LAMBERT)
    // Lambert Shading: Per-pixel diffuse lighting
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    
    // Ambient + Directional Lights
    FinalColor = ComputeLambertLighting(BaseColor, Normal);
    
    // Local Lights (Point, Spot)
    for (int i = 0; i < NumLocalLights; ++i)
    {
        FinalColor.rgb += LocalLightLambert(g_LightBuffer[i], Normal, WorldPos);
    }
    FinalColor.rgb = saturate(FinalColor.rgb);

#elif defined(LIGHTING_MODEL_PHONG)
    // Blinn-Phong Shading: Per-pixel specular lighting
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);

    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);
    
    // Ambient + Directional Lights
    FinalColor = ComputeBlinnPhongLighting(BaseColor, Normal, MaterialParam, WorldPos, ViewDir);

    // Local Lights (Point, Spot)
    for (int j = 0; j < NumLocalLights; ++j)
    {
        FinalColor.rgb += LocalLightBlinnPhong(g_LightBuffer[j], Normal, WorldPos, ViewDir, Shininess, SpecularStrength);
    }
    FinalColor.rgb = saturate(FinalColor.rgb);

#elif defined(LIGHTING_MODEL_WORLDNORMAL)
    // Debug view mode: World Normal
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    FinalColor = float4(Normal * 0.5f + 0.5f, 1.0f);

#elif defined(LIGHTING_MODEL_UNLIT)
    // Unlit: Base color only
    FinalColor = BaseColor;

#else
    // Default: Base color only
    FinalColor = BaseColor;
#endif

    return FinalColor;
}
