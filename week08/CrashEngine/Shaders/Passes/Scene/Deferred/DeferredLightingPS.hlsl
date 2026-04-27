/*
    DeferredLightingPS.hlsl는 deferred lighting pass의 셰이더입니다.

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

/*
    Available preprocessor defines:
    - LIGHTING_MODEL_GOURAUD
    - LIGHTING_MODEL_LAMBERT
    - LIGHTING_MODEL_BLINNPHONG
    - LIGHTING_MODEL_WORLDNORMAL
    - LIGHTING_MODEL_UNLIT
    - ENABLE_LIGHT_EVAL_COUNTER
*/

#include "../../../Render/Scene/Shared/ViewModeCommon.hlsli"

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
    return ResolveViewModeSurfaceValue(g_BaseColorTex, g_ModifiedBaseColorTex, UV);
}

float4 ResolveSurface1(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface1Tex, g_ModifiedSurface1Tex, UV);
}

float4 ResolveSurface2(float2 UV)
{
    return ResolveViewModeSurfaceValue(g_Surface2Tex, g_ModifiedSurface2Tex, UV);
}

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float SceneDepthValue = SceneDepth.Sample(PointClampSampler, UV).r;
    if (SceneDepthValue <= 0.0f)
    {
        // Leave the frame clear color visible on background pixels with no scene geometry.
        discard;
    }

    float4 BaseColor = ResolveBaseColor(UV);
    float4 FinalColor = BaseColor;

#if defined(LIGHTING_MODEL_GOURAUD)
    float4 GouraudLighting = ResolveSurface1(UV);
    FinalColor = ComputeGouraudLighting(BaseColor, GouraudLighting);

#elif defined(LIGHTING_MODEL_LAMBERT)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    float3 TotalLight = ComputeLambertGlobalLight(Normal);
    float3 TotalLocalLight = 0.0f;

    // Deferred는 global light를 먼저 구한 뒤, 아래 타일 light 목록에서
    // local light만 추가로 누적합니다. 마지막에 BaseColor를 한 번만 곱해서
    // forward의 Lambert 수식과 같은 형태를 유지합니다.
    FinalColor = BaseColor;

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
                    TotalLocalLight += LocalLightLambertTerm(g_LightBuffer[GlobalPointLightIndex], Normal, WorldPos, Input.position);
#if ENABLE_LIGHT_EVAL_COUNTER
                    InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
                }
            }
        }
    }
    FinalColor.rgb = BaseColor.rgb * saturate(TotalLight + TotalLocalLight);

#elif defined(LIGHTING_MODEL_BLINNPHONG)
    float3 Normal = normalize(DecodeNormal(ResolveSurface1(UV)));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    float3 WorldPos = ReconstructWorldPositionFromSceneDepth(UV);
    float3 ViewDir = normalize(CameraWorldPos - WorldPos);
    FLocalBlinnPhongTerm TotalLight = ComputeBlinnPhongGlobalLight(Normal, MaterialParam, ViewDir);
    FLocalBlinnPhongTerm TotalLocalLight = (FLocalBlinnPhongTerm)0;

    float Shininess = max(MaterialParam.x, 1.0f);
    float SpecularStrength = max(MaterialParam.y, 0.0f);

    // Deferred는 global light를 먼저 구한 뒤, 아래 타일 light 목록에서
    // local light를 diffuse/specular로 분리해 누적합니다.
    // 마지막 합성도 forward와 같은 형태로 맞춰 경로 간 밝기 차이를 줄입니다.
    FinalColor = BaseColor;

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
                        SpecularStrength,
                        Input.position);
                    TotalLocalLight.Diffuse += LocalTerm.Diffuse;
                    TotalLocalLight.Specular += LocalTerm.Specular;
#if ENABLE_LIGHT_EVAL_COUNTER
                    InterlockedAdd(GlobalLightEvalCounter[0], 1);
#endif
                }
            }
        }
    }
    FinalColor.rgb = BaseColor.rgb * saturate(TotalLight.Diffuse + TotalLocalLight.Diffuse)
        + (TotalLight.Specular + TotalLocalLight.Specular) * 0.2f;

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
