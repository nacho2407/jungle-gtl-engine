#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"
#include "../Common/LightCulling.hlsli"
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

float ComputeDecalLightingFactor(PSInput Input, float3 Normal)
{
#if LIGHTING_MODEL_GOURAUD
    return saturate(max(Input.LitColor.r, max(Input.LitColor.g, Input.LitColor.b)));
#else
    float3 Lighting = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));

#if LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    const float DirectionalShadow = CalculateDirectionalShadow(float4(Input.WorldPos, 1.0f), ShadowSampler, ShadowMap, SampleState, VSMMap);
    Lighting += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), Normal) * DirectionalShadow;

    FVisibleLightList VisibleLights = GetVisibleLightList(Input.ClipPos);

    for (uint i = 0; i < VisibleLights.Count; ++i)
    {
        uint LightIndex = GetVisibleLightIndex(VisibleLights, i);
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
