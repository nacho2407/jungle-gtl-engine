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
    uint2 PixelCoord = uint2(Input.position.xy);
    uint2 TileCoord = PixelCoord / TileSize; // 각 성분별 나눔
    uint TilesX = (ScreenSize.x + TileSize.x - 1) / TileSize.x; // 한 행에 존재하는 타일 수
    uint FlatTileIndex = TileCoord.x + TileCoord.y * TilesX;
    
    int BucketsPerTile = MAX_LIGHTS_PER_TILE / 32;
    int StartIndex = FlatTileIndex * BucketsPerTile;
    for (int Bucket = 0; Bucket < BucketsPerTile; ++Bucket)
    {
        int PointMask = PerTileLightMask[StartIndex + Bucket];
        for (int bit = 0; bit < 32; ++bit)
        {
            if (PointMask & (1u << bit))
            {
                int GlobalPointLightIndex = Bucket * 32 + bit;
                if (GlobalPointLightIndex < NumLocalLights)
                {
                    float3 LocalTerm = LocalLightLambertTerm(g_LightBuffer[GlobalPointLightIndex], Normal, WorldPos);
                    FinalColor.rgb += BaseColor.rgb * LocalTerm;
                }

            }
        }
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
    uint2 PixelCoord = uint2(Input.position.xy);
    uint2 TileCoord = PixelCoord / TileSize; // 각 성분별 나눔
    uint TilesX = (ScreenSize.x + TileSize.x - 1) / TileSize.x; // 한 행에 존재하는 타일 수
    uint FlatTileIndex = TileCoord.x + TileCoord.y * TilesX;
    
    int BucketsPerTile = MAX_LIGHTS_PER_TILE / 32;
    int StartIndex = FlatTileIndex * BucketsPerTile;
    for (int Bucket = 0; Bucket < BucketsPerTile; ++Bucket)
    {
        int PointMask = PerTileLightMask[StartIndex + Bucket];
        for (int bit = 0; bit < 32; ++bit)
        {
            if (PointMask & (1u << bit))
            {
                int GlobalPointLightIndex = Bucket * 32 + bit;
                if (GlobalPointLightIndex < NumLocalLights)
                {
                    FLocalBlinnPhongTerm LocalTerm = LocalLightBlinnPhongTerm(
                        g_LightBuffer[GlobalPointLightIndex],
                        Normal,
                        WorldPos,
                        ViewDir,
                        Shininess,
                        SpecularStrength);
                    FinalColor.rgb += BaseColor.rgb * LocalTerm.Diffuse + LocalTerm.Specular;
                }

            }
        }
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
