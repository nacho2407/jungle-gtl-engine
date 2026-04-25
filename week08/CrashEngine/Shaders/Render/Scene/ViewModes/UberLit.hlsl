/*
    UberLit.hlsl는 에디터 뷰 모드용 셰이딩 분기를 처리합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 텍스처/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언하는 추가 리소스: t0, t1, t2, t3, t4, t5, u1
*/

#include "../Shared/OpaquePassTypes.hlsli"
#include "../../../Surface/SurfacePacking.hlsli"
#include "../Lighting/LightingCommon.hlsli"

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);

Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);

// t20 ~ t24: Shadow Maps (Placeholder for RenderDoc verification)
TextureCube g_ShadowMap0 : register(t20);
TextureCube g_ShadowMap1 : register(t21);
TextureCube g_ShadowMap2 : register(t22);
TextureCube g_ShadowMap3 : register(t23);
TextureCube g_ShadowMap4 : register(t24);

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

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float4 BaseColor = ResolveBaseColor(UV);
    float4 FinalColor = BaseColor;
    
    // 임시코드: 셰이더 컴파일 최적화로 바인딩 목록에서 제거되는 것 방지
    // RenderDoc에서 텍스쳐 전달되는 것 확인용
    // float dummy = g_ShadowMap0.Sample(PointClampSampler, float3(0,0,0)).r +
    //               g_ShadowMap1.Sample(PointClampSampler, float3(0,0,0)).r +
    //               g_ShadowMap2.Sample(PointClampSampler, float3(0,0,0)).r +
    //               g_ShadowMap3.Sample(PointClampSampler, float3(0,0,0)).r +
    //               g_ShadowMap4.Sample(PointClampSampler, float3(0,0,0)).r;
    // return dummy * 0.000001f;
    
#if defined(LIGHTING_MODEL_GOURAUD)
    float4 GouraudLighting = ResolveSurface1(UV);
    FinalColor = ComputeGouraudLighting(BaseColor, GouraudLighting);

#elif defined(LIGHTING_MODEL_LAMBERT)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);

    FinalColor = ComputeLambertLighting(BaseColor, Normal);

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

#elif defined(LIGHTING_MODEL_BLINNPHONG)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);

    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    FinalColor = ComputeBlinnPhongLighting(BaseColor, Normal, MaterialParam, WorldPos, ViewDir);

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
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    FinalColor = float4(Normal * 0.5f + 0.5f, 1.0f);

#elif defined(LIGHTING_MODEL_UNLIT)
    FinalColor = BaseColor;

#else
    FinalColor = BaseColor;
#endif

    return FinalColor;
}
