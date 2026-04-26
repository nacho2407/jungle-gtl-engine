#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/ForwardLightData.hlsli"

cbuffer ClusterHeatMapCB : register(b2)
{
    float MaxLightCount;
    float3 _pad;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    PS_Input_UV output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float3 HeatColor(float t)
{
    t = saturate(t);

    float3 cold = float3(0.02f, 0.05f, 0.15f);
    float3 blue = float3(0.0f, 0.25f, 1.0f);
    float3 cyan = float3(0.0f, 0.9f, 1.0f);
    float3 yellow = float3(1.0f, 0.95f, 0.0f);
    float3 red = float3(1.0f, 0.05f, 0.0f);

    if (t < 0.25f)
        return lerp(cold, blue, t / 0.25f);
    if (t < 0.5f)
        return lerp(blue, cyan, (t - 0.25f) / 0.25f);
    if (t < 0.75f)
        return lerp(cyan, yellow, (t - 0.5f) / 0.25f);
    return lerp(yellow, red, (t - 0.75f) / 0.25f);
}

uint DepthToClusterSlice(float viewDepth)
{
    float safeDepth = clamp(viewDepth, CullState.NearZ, CullState.FarZ);
    float logDepth = log(safeDepth / CullState.NearZ) / log(CullState.FarZ / CullState.NearZ);
    return min((uint) floor(logDepth * CullState.ClusterZ), CullState.ClusterZ - 1);
}

uint ComputeClusterIndex(float4 pixelPos, float2 uv)
{
    float depth = SceneDepth.Load(int3(pixelPos.xy, 0));
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 viewPos = mul(clipPos, InvProj);
    viewPos /= max(abs(viewPos.w), 0.00001f);

    uint tileX = min((uint) (pixelPos.x / CullState.ScreenWidth * CullState.ClusterX), CullState.ClusterX - 1);
    uint tileY = min((uint) (pixelPos.y / CullState.ScreenHeight * CullState.ClusterY), CullState.ClusterY - 1);
    uint sliceZ = DepthToClusterSlice(abs(viewPos.z));

    return sliceZ * CullState.ClusterX * CullState.ClusterY
        + tileY * CullState.ClusterX
        + tileX;
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float depth = SceneDepth.Load(int3(input.position.xy, 0));
    if (depth <= 0.00001f)
        discard;

    uint clusterIdx = ComputeClusterIndex(input.position, input.uv);
    uint lightCount = g_ClusterLightGrid[clusterIdx].y;
    if (lightCount == 0)
        discard;

    float scale = max(MaxLightCount, 1.0f);
    float normalizedCount = lightCount / scale;
    float3 color = HeatColor(normalizedCount);

    return float4(color, 0.45f);
}
