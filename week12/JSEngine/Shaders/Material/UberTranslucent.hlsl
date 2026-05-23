#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"

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
#if BONE_WEIGHT_HEATMAP
    float BoneWeightHeat : TEXCOORD6;
#endif
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor : TEXCOORD3;
#elif HAS_NORMAL_MAP
    float4 WorldTangent : TEXCOORD5;
#endif
};

#if HAS_NORMAL_MAP
float3 PerturbNormal(float3 worldNormal, float4 worldTangent, float2 uv)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent.xyz - dot(worldTangent.xyz, N) * N);
    float3 B = cross(N, T) * worldTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    float3 tn = BumpMap.Sample(SampleState, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(tn, TBN));
}
#endif

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

    uint LightsToIterate = LightCount;
    uint LightOffset = 0;
#if CULLING_MODEL_CLUSTERED
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint numTilesY = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
    float z = (IsOrthographic) ? NearZ + input.ClipPos.z * (FarZ - NearZ) : abs(Projection[3][2] / (input.ClipPos.z - Projection[0][2]));
    uint sliceIndex = clamp(uint(log(z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
    uint2 clusterData = TileBuffer[(sliceIndex * numTilesY + tileCoord.y) * numTilesX + tileCoord.x];
    LightOffset = clusterData.x;
    LightsToIterate = clusterData.y;
#elif CULLING_MODEL_TILED
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];
    LightOffset = tileData.x;
    LightsToIterate = tileData.y;
#endif

    for (uint i = 0; i < LightsToIterate; ++i)
    {
#if CULLING_MODEL_CLUSTERED || CULLING_MODEL_TILED
        uint lightIndex = CulledIndexBuffer[LightOffset + i];
#else
        uint lightIndex = i;
#endif
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
    N = PerturbNormal(input.WorldNormal, input.WorldTangent, input.UV);
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
