#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"
#include "../Common/LightCulling.hlsli"
#include "../Common/NormalMapping.hlsli"

cbuffer StaticMeshBuffer : register(b2)
{
    float3 AmbientColor;
    float padding0;

    float3 DiffuseColor;
    float padding1;

    float3 SpecularColor;
    float Shininess;

    float2 ScrollUV;
    float2 padding2;

    float3 EmissiveColor;
    float Opacity;
};

#if HAS_DIFFUSE_MAP
Texture2D DiffuseMap : register(t0);
#endif
#if HAS_NORMAL_MAP
Texture2D BumpMap : register(t1);
#endif
#if HAS_EMISSIVE_MAP
Texture2D EmissiveMap : register(t2);
#endif
#if HAS_SPECULAR_MAP
Texture2D SpecularMap : register(t3);
#endif

SamplerState SampleState : register(s0);

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor : TEXCOORD3;
#elif HAS_NORMAL_MAP
    float4 WorldTangent : TEXCOORD5;
#endif
};

float3 AccumulateSurfaceLight(PSInput input, float3 normal, float3 specularFactor)
{
#if LIGHTING_MODEL_GOURAUD
    return input.LitColor;
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    float3 accumulatedLight = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    float3 V = normalize(CameraPosition - input.WorldPos);
    if (IsOrthographic > 0.5f)
    {
        V = normalize(-float3(View[0].xyz));
    }

#if LIGHTING_MODEL_LAMBERT
    accumulatedLight += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), normal);
#elif LIGHTING_MODEL_PHONG
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), normal, input.WorldPos, V, Shininess, specularFactor);
#endif

    FVisibleLightList VisibleLights = GetVisibleLightList(input.ClipPos);

    for (uint i = 0; i < VisibleLights.Count; ++i)
    {
        uint lightIndex = GetVisibleLightIndex(VisibleLights, i);
        LightInfo light = Lights[lightIndex];
#if LIGHTING_MODEL_LAMBERT
        accumulatedLight += light.Type == 0
            ? CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), normal, input.WorldPos)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), normal, input.WorldPos);
#elif LIGHTING_MODEL_PHONG
        accumulatedLight += light.Type == 0
            ? CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), normal, input.WorldPos, V, Shininess, specularFactor)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), normal, input.WorldPos, V, Shininess, specularFactor);
#endif
    }
    return accumulatedLight;
#else
    return float3(1.0f, 1.0f, 1.0f);
#endif
}

float4 mainPS(PSInput input) : SV_TARGET0
{
    float4 DiffuseTex = float4(1.0f, 1.0f, 1.0f, 1.0f);
#if HAS_DIFFUSE_MAP
    DiffuseTex = DiffuseMap.Sample(SampleState, input.UV);
#endif

    float3 SpecularFactor = SpecularColor;
#if HAS_SPECULAR_MAP
    SpecularFactor *= SpecularMap.Sample(SampleState, input.UV).rgb;
#endif

    float3 Emissive = EmissiveColor;
#if HAS_EMISSIVE_MAP
    Emissive *= EmissiveMap.Sample(SampleState, input.UV).rgb;
#endif

    float3 N = normalize(input.WorldNormal);
#if HAS_NORMAL_MAP && !LIGHTING_MODEL_GOURAUD
    N = PerturbNormal(BumpMap, SampleState, input.WorldNormal, input.WorldTangent, input.UV);
#endif

    float3 Lit = AccumulateSurfaceLight(input, N, SpecularFactor);
    float Alpha = saturate(Opacity * DiffuseTex.a);
    float3 Color = DiffuseColor * DiffuseTex.rgb * Lit + Emissive * DiffuseTex.rgb;

    if (bIsWireframe > 0.5f)
    {
        Color = WireframeRGB;
        Alpha = 1.0f;
    }

    return float4(Color, Alpha);
}
