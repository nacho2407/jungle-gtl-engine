
/*
    UberLit.hlsl는 에디터 뷰 모드용 셰이딩 분기를 처리합니다.

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
    - 이 파일에서 직접 선언한 슬롯: t0, t1, t2, t3, t4, t5, u1
*/

#include "../../../Common/Surface/CommonTypes.hlsli"
#include "../../../Common/Surface/SurfaceData.hlsli"
#include "../../../Common/Lighting/LightingCommon.hlsli"

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);

Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);

#ifndef ENABLE_LIGHT_EVAL_COUNTER
#define ENABLE_LIGHT_EVAL_COUNTER 0
#endif

#if ENABLE_LIGHT_EVAL_COUNTER
RWBuffer<uint> GlobalLightEvalCounter : register(u1);
#endif

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

// 정점 입력을 화면 공간 출력으로 변환하는 버텍스 셰이더입니다.
PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
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
    uint2 TileCoord = PixelCoord / TileSize;
    uint TilesX = (ScreenSize.x + TileSize.x - 1) / TileSize.x;
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
#if ENABLE_LIGHT_EVAL_COUNTER
                    InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
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
    uint2 TileCoord = PixelCoord / TileSize;
    uint TilesX = (ScreenSize.x + TileSize.x - 1) / TileSize.x;
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
#if ENABLE_LIGHT_EVAL_COUNTER
                    InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
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

