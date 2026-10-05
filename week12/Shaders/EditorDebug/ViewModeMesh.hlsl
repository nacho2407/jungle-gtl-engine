#include "../Common/Common.hlsli"
#include "../Common/SkeletalSkinning.hlsli"
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
    float padding3;
};

cbuffer BoneWeightHeatmapBuffer : register(b6)
{
    int SelectedBoneIndex;
    uint bBoneWeightHeatmapEnabled;
    float2 BoneWeightHeatmapPadding;
};

cbuffer DebugViewModeResolveConstants : register(b7)
{
    uint ViewMode;
    float3 ViewModePadding;
};

#if HAS_DIFFUSE_MAP
Texture2D DiffuseMap : register(t0);
#endif
#if HAS_NORMAL_MAP
Texture2D BumpMap : register(t1);
#endif

SamplerState SampleState : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct SkeletalVSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 WorldTangent : TEXCOORD3;
    float BoneWeightHeat : TEXCOORD4;
};

PSInput mainVS(VSInput input)
{
    PSInput output;
    output.WorldPos = mul(float4(input.Position, 1.0f), Model).xyz;
    output.ClipPos = ApplyMVP(input.Position);
    output.UV = input.UV + ScrollUV;
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3)WorldInvTrans)), input.Tangent.w);
    output.BoneWeightHeat = 0.0f;
    return output;
}

float GetSelectedBoneWeight(uint4 BoneIndices, float4 BoneWeights)
{
    if (bBoneWeightHeatmapEnabled == 0 || SelectedBoneIndex < 0)
        return 0.0f;

    float selectedWeight = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if ((int)BoneIndices[i] == SelectedBoneIndex)
            selectedWeight += BoneWeights[i];
    }
    return saturate(selectedWeight);
}

PSInput SkeletalMeshVS(SkeletalVSInput input)
{
    FSkinningResult skinned = ApplyLinearBlendSkinning(
        input.Position,
        input.Normal,
        input.Tangent.xyz,
        input.BoneIndices,
        input.BoneWeights);

    VSInput passThrough;
    passThrough.Position = skinned.Position;
    passThrough.Color = input.Color;
    passThrough.Normal = skinned.Normal;
    passThrough.UV = input.UV;
    passThrough.Tangent = float4(skinned.Tangent, input.Tangent.w);

    PSInput output = mainVS(passThrough);
    output.BoneWeightHeat = GetSelectedBoneWeight(input.BoneIndices, input.BoneWeights);
    return output;
}

float3 PerturbNormal(float3 worldNormal, float4 worldTangent, float2 uv)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent.xyz - dot(worldTangent.xyz, N) * N);
    float3 B = cross(N, T) * worldTangent.w;
    float3x3 TBN = float3x3(T, B, N);
#if HAS_NORMAL_MAP
    float3 tn = BumpMap.Sample(SampleState, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(tn, TBN));
#else
    return N;
#endif
}

float3 GetHeatmapColor(float weight)
{
    float3 color;
    color.r = smoothstep(0.4f, 0.7f, weight);
    color.g = smoothstep(0.0f, 0.4f, weight) - smoothstep(0.7f, 1.0f, weight);
    color.b = 1.0f - smoothstep(0.0f, 0.4f, weight);
    return color;
}

float3 GetBoneWeightHeatmapColor(float weight)
{
    weight = saturate(weight);

    const float3 noneColor = float3(1.0f, 0.0f, 1.0f);
    const float3 lowColor = float3(0.0f, 0.25f, 1.0f);
    const float3 midColor = float3(0.0f, 0.9f, 0.35f);
    const float3 highColor = float3(1.0f, 0.85f, 0.0f);
    const float3 maxColor = float3(1.0f, 0.05f, 0.0f);

    if (weight < 0.25f)
        return lerp(noneColor, lowColor, weight / 0.25f);
    if (weight < 0.5f)
        return lerp(lowColor, midColor, (weight - 0.25f) / 0.25f);
    if (weight < 0.75f)
        return lerp(midColor, highColor, (weight - 0.5f) / 0.25f);
    return lerp(highColor, maxColor, (weight - 0.75f) / 0.25f);
}

float4 mainPS(PSInput input) : SV_TARGET0
{
#if HAS_DIFFUSE_MAP
    float4 diffuseTex = DiffuseMap.Sample(SampleState, input.UV);
    clip(diffuseTex.a - 0.001f);
#endif

    if (ViewMode == 7)
    {
        float3 normal = PerturbNormal(input.WorldNormal, input.WorldTangent, input.UV);
        return float4(normal * 0.5f + 0.5f, 1.0f);
    }

    if (ViewMode == 5)
    {
        return float4(GetBoneWeightHeatmapColor(input.BoneWeightHeat), 1.0f);
    }

#if CULLING_MODEL_CLUSTERED
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint numTilesY = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
    float z = (IsOrthographic > 0.5f) ? NearZ + input.ClipPos.z * (FarZ - NearZ) : abs(Projection[3][2] / (input.ClipPos.z - Projection[0][2]));
    uint sliceIndex = clamp(uint(log(z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
    uint2 clusterData = TileBuffer[(sliceIndex * numTilesY + tileCoord.y) * numTilesX + tileCoord.x];
    float weight = saturate(float(clusterData.y) / 64.0f);
#elif CULLING_MODEL_TILED
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];
    float weight = saturate(float(tileData.y) / 64.0f);
#else
    float weight = 0.0f;
#endif

    float3 heatmapColor = GetHeatmapColor(weight);
    uint2 pixelInTile = uint2(input.ClipPos.xy) % TILE_SIZE;
    if (pixelInTile.x == 0 || pixelInTile.y == 0)
        heatmapColor *= 0.5f;
    return float4(heatmapColor, 1.0f);
}
