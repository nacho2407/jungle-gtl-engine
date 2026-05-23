#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"
#include "../Common/ShadowFunction.hlsli"

// Projection decal bindings:
// b0 FrameConstants, b1 recipient object, b3 light constants, b4 shadow constants, b8 decal constants.
// t0 DiffuseMap, t4 Lights, t5/t6 light culling, t10/t11/t12 shadow maps, t14/t15 shadow metadata.

cbuffer ProjectionDecalConstants : register(b8)
{
    row_major matrix InvDecalWorld;
    float4 DecalColorTint;
};

#if HAS_DIFFUSE_MAP
Texture2D DiffuseMap : register(t0);
#endif

Texture2D ShadowMap : register(t10);
Texture2D VSMMap : register(t11);
TextureCubeArray<float> PointShadowCube : register(t12);

SamplerState SampleState : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor : TEXCOORD3;
#endif
};

float GetCascadeSplitFarValue(uint CascadeIndex)
{
    return (CascadeIndex == 0) ? CascadeSplitFar.x :
           (CascadeIndex == 1) ? CascadeSplitFar.y :
           (CascadeIndex == 2) ? CascadeSplitFar.z :
                                 CascadeSplitFar.w;
}

float GetCameraDepthForCSM(float3 WorldPos)
{
    float4 ViewPos = mul(float4(WorldPos, 1.0f), View);
    return ViewPos.x;
}

uint SelectDirectionalCascade(float CameraDepth)
{
    for (uint i = 0; i < DirectionalCascadeCount; ++i)
    {
        if (CameraDepth <= GetCascadeSplitFarValue(i))
        {
            return i;
        }
    }
    return MAX_DIRECTIONAL_CASCADE_COUNT;
}

float ComputeBias(float LightSpaceZ, float ConstantBias, float SlopeScaleBias)
{
    float dz_dx = ddx(LightSpaceZ);
    float dz_dy = ddy(LightSpaceZ);
    float slope = max(abs(dz_dx), abs(dz_dy));
    return ConstantBias + slope * SlopeScaleBias;
}

float3 ComputeShadowCoordCascade(float4 WorldPos, int CascadeIndex)
{
    FAtlasShadowData ShadowData = AtlasShadowDatas[DirectionalShadowStartIndex + CascadeIndex];
    float4 ShadowCoord = mul(WorldPos, ShadowData.ShadowViewProj);
    if (abs(ShadowCoord.w) < 1e-5f)
    {
        return 1.0f;
    }
    return ShadowCoord.xyz / ShadowCoord.w;
}

#ifdef SHADOW_MAP_CSM
float CalculateDirectionalShadow(float4 WorldPos)
{
    if (DirectionalCascadeCount == 0 || DirectionalCascadeCount == 0)
    {
        return 1.0f;
    }

    float CameraDepth = GetCameraDepthForCSM(WorldPos.xyz);
    uint CascadeIndex = SelectDirectionalCascade(CameraDepth);

    if (CascadeIndex >= DirectionalCascadeCount)
    {
        return 1.0f;
    }

    const float BlendAreaRatio = 0.1f;
    float CascadeFar = GetCascadeSplitFarValue(CascadeIndex);
    float CascadeNear = (CascadeIndex == 0) ? 0.0f : GetCascadeSplitFarValue(CascadeIndex - 1);
    float BlendWidth = (CascadeFar - CascadeNear) * BlendAreaRatio;

    float BlendFactor = saturate((CameraDepth - (CascadeFar - BlendWidth)) / BlendWidth);

    float3 projCoords = ComputeShadowCoordCascade(WorldPos, CascadeIndex);
    
    float2 shadowUV = float2(
        projCoords.x * 0.5f + 0.5f,
        -projCoords.y * 0.5f + 0.5f
    );

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    FAtlasShadowData cascadeShadowData = AtlasShadowDatas[DirectionalShadowStartIndex + CascadeIndex];
    
    float totalBias = ComputeBias(projCoords.z, cascadeShadowData.ConstantBias, cascadeShadowData.SlopedBias);
#if SHADOW_MAP_VSM
    float ShadowFactor = ComputeShadowVSM(projCoords, cascadeShadowData.ScaleOffset, VSMMap, SampleState, 0.0001);
#else
    float ShadowFactor = ComputeShadowPCF(projCoords, cascadeShadowData.ScaleOffset, (int)cascadeShadowData.ShadowSoftness, ShadowSampler, ShadowMap, totalBias);
#endif
    if (CascadeIndex < DirectionalCascadeCount - 1)
    {
        int NextCascade = CascadeIndex + 1;
        float3 NextCascadeProjCoords = ComputeShadowCoordCascade(WorldPos, NextCascade);
        FAtlasShadowData nextCascadeShadowData = AtlasShadowDatas[DirectionalShadowStartIndex + NextCascade];
        float nextTotalBias = ComputeBias(NextCascadeProjCoords.z, nextCascadeShadowData.ConstantBias, nextCascadeShadowData.SlopedBias);

#if SHADOW_MAP_VSM
        float NextCascadeShadowFactor = ComputeShadowVSM(NextCascadeProjCoords, nextCascadeShadowData.ScaleOffset, VSMMap, SampleState, 0.0001);
#else
        float NextCascadeShadowFactor = ComputeShadowPCF(NextCascadeProjCoords, nextCascadeShadowData.ScaleOffset, (int)nextCascadeShadowData.ShadowSoftness, ShadowSampler, ShadowMap, nextTotalBias);
#endif
        ShadowFactor = lerp(ShadowFactor, NextCascadeShadowFactor, BlendFactor);
    }
    
    return ShadowFactor;
}
#elif SHADOW_MAP_PSM
float CalculateDirectionalShadow(float4 WorldPos)
{
    float4 CamClip = mul(WorldPos, VirtualViewProj);
    if (abs(CamClip.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 Post = CamClip.xyz / CamClip.w;
    float4 ShadowCoord = mul(float4(Post, 1.0f), ShadowViewProj);
    if (abs(ShadowCoord.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 ProjCoords = ShadowCoord.xyz / ShadowCoord.w;
    float2 ShadowUV = float2(ProjCoords.x * 0.5f + 0.5f, -ProjCoords.y * 0.5f + 0.5f);
    if (ShadowUV.x < 0.0f || ShadowUV.x > 1.0f ||
        ShadowUV.y < 0.0f || ShadowUV.y > 1.0f ||
        ProjCoords.z < 0.0f || ProjCoords.z > 1.0f)
    {
        return 1.0f;
    }

    FAtlasShadowData ShadowData = AtlasShadowDatas[DirectionalShadowStartIndex];
    float TotalBias = ComputeBias(ProjCoords.z, ShadowData.ConstantBias, ShadowData.SlopedBias);
#if SHADOW_MAP_VSM
    return ComputeShadowVSM(ProjCoords, ShadowData.ScaleOffset, VSMMap, SampleState, 0.0001f);
#else
    return ComputeShadowPCF(ProjCoords, ShadowData.ScaleOffset, (int)ShadowData.ShadowSoftness, ShadowSampler, ShadowMap, TotalBias);
#endif
}
#else
float CalculateDirectionalShadow(float4 WorldPos)
{
    return 1.0f;
}
#endif

float ComputeDecalLightingFactor(PSInput Input, float3 Normal)
{
#if LIGHTING_MODEL_GOURAUD
    return saturate(max(Input.LitColor.r, max(Input.LitColor.g, Input.LitColor.b)));
#else
    float3 Lighting = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));

#if LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    const float DirectionalShadow = CalculateDirectionalShadow(float4(Input.WorldPos, 1.0f));
    Lighting += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), Normal) * DirectionalShadow;

    uint LightsToIterate = LightCount;
#if CULLING_MODEL_CLUSTERED
    uint2 TileCoord = uint2(Input.ClipPos.xy) / TILE_SIZE;
    uint NumTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint NumTilesY = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
    float Z = (IsOrthographic) ? NearZ + Input.ClipPos.z * (FarZ - NearZ) : abs(Projection[3][2] / (Input.ClipPos.z - Projection[0][2]));
    uint SliceIndex = clamp(uint(log(Z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
    uint2 ClusterData = TileBuffer[(SliceIndex * NumTilesY + TileCoord.y) * NumTilesX + TileCoord.x];
    LightsToIterate = ClusterData.y;
#elif CULLING_MODEL_TILED
    uint2 TileCoord = uint2(Input.ClipPos.xy) / TILE_SIZE;
    uint NumTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 TileData = TileBuffer[TileCoord.y * NumTilesX + TileCoord.x];
    LightsToIterate = TileData.y;
#endif

    for (uint i = 0; i < LightsToIterate; ++i)
    {
#if CULLING_MODEL_CLUSTERED
        uint LightIndex = CulledIndexBuffer[ClusterData.x + i];
#elif CULLING_MODEL_TILED
        uint LightIndex = CulledIndexBuffer[TileData.x + i];
#else
        uint LightIndex = i;
#endif
        LightInfo Light = Lights[LightIndex];
        const float LightShadow = ComputeShadowAtlas(LightIndex, float4(Input.WorldPos, 1.0f), ShadowSampler, ShadowMap, SampleState, PointShadowCube);
        float3 LightContribution = Light.Type == 0
            ? CalcSpotlightLambert(Light, float3(1.0f, 1.0f, 1.0f), Normal, Input.WorldPos)
            : CalcPointLambert(Light, float3(1.0f, 1.0f, 1.0f), Normal, Input.WorldPos);
        Lighting += LightContribution * LightShadow;
    }
#endif

    return saturate(max(Lighting.r, max(Lighting.g, Lighting.b)));
#endif
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    float4 DecalLocalPos = mul(float4(Input.WorldPos, 1.0f), InvDecalWorld);
    if (any(abs(DecalLocalPos.xyz) > 0.5f))
    {
        discard;
    }

    float2 DecalUV = DecalLocalPos.yz + 0.5f;
    DecalUV.y = 1.0f - DecalUV.y;

    float4 DecalSample = float4(1.0f, 1.0f, 1.0f, 1.0f);
#if HAS_DIFFUSE_MAP
    DecalSample = DiffuseMap.Sample(SampleState, DecalUV);
#endif

    float Alpha = DecalSample.a * DecalColorTint.a;
    if (Alpha <= 0.001f)
    {
        discard;
    }

    float3 Normal = normalize(Input.WorldNormal);
    float LightingFactor = ComputeDecalLightingFactor(Input, Normal);
    float LitAlpha = Alpha * LightingFactor;
    return float4(DecalSample.rgb * DecalColorTint.rgb, LitAlpha);
}
