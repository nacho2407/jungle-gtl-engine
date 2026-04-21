#ifndef FORWARD_LIGHTING_HLSLI
#define FORWARD_LIGHTING_HLSLI

#include "Common/ForwardLightData.hlsli"

float CalcAttenuation(float dist, float radius, float falloff)
{
    float ratio = saturate(dist / max(radius, 0.0001f));
    return pow(1.0f - ratio, falloff);
}

float3 CalcAmbient(float3 lightColor, float intensity)
{
    return lightColor * intensity;
}

float3 CalcDirectionalDiffuse(float3 lightColor, float3 lightDir, float intensity, float3 N)
{
    float NdotL = saturate(dot(N, -lightDir));
    return lightColor * intensity * NdotL;
}

float3 CalcDirectionalSpecular(float3 lightColor, float3 lightDir, float intensity,
                               float3 N, float3 V, float shininess)
{
    float3 H = normalize(-lightDir + V);
    float NdotH = saturate(dot(N, H));
    return lightColor * intensity * pow(NdotH, max(shininess, 1.0f));
}

float3 GetHeatmapColor(float value)
{
    float3 color;
    color.r = saturate(min(4.0 * value - 1.5, -4.0 * value + 4.5));
    color.g = saturate(min(4.0 * value - 0.5, -4.0 * value + 3.5));
    color.b = saturate(min(4.0 * value + 0.5, -4.0 * value + 2.5));
    return color;
}

uint DepthToClusterSlice(float viewDepth)
{
    float safeDepth = clamp(viewDepth, CullState.NearZ, CullState.FarZ);
    float logDepth = log(safeDepth / CullState.NearZ) / log(CullState.FarZ / CullState.NearZ);
    return min((uint) floor(logDepth * CullState.ClusterZ), CullState.ClusterZ - 1);
}

uint ComputeClusterIndex(float4 screenPos, float3 worldPos)
{
    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    uint tileX = min((uint) (screenPos.x / CullState.ScreenWidth * CullState.ClusterX), CullState.ClusterX - 1);
    uint tileY = min((uint) (screenPos.y / CullState.ScreenHeight * CullState.ClusterY), CullState.ClusterY - 1);
    uint sliceZ = DepthToClusterSlice(abs(viewPos.z));

    return sliceZ * CullState.ClusterX * CullState.ClusterY
        + tileY * CullState.ClusterX
        + tileX;
}

float3 CalcLightDiffuse(FLightInfo light, float3 worldPos, float3 N)
{
    float3 L = light.Position - worldPos;
    float dist = length(L);
    L = normalize(L);

    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);
    float NdotL = saturate(dot(N, L));

    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    return light.Color.rgb * light.Intensity * NdotL * atten * spotFactor;
}

float3 CalcLightSpecular(FLightInfo light, float3 worldPos, float3 N, float3 V, float shininess)
{
    float3 L = normalize(light.Position - worldPos);
    float dist = length(light.Position - worldPos);
    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);

    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    return light.Color.rgb * light.Intensity * pow(NdotH, max(shininess, 1.0f)) * atten * spotFactor;
}

void AccumulatePointSpotDiffuse(float3 worldPos, float3 N, float4 screenPos, inout float3 result)
{
    if (LightCullingMode == LIGHT_CULLING_TILE && NumTilesX > 0 && NumTilesY > 0)
    {
        uint2 tileCoord = min(uint2(screenPos.xy) / TILE_SIZE, uint2(NumTilesX - 1, NumTilesY - 1));
        uint tileIdx = tileCoord.y * NumTilesX + tileCoord.x;
        uint2 gridData = TileLightGrid[tileIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcLightDiffuse(AllLights[TileLightIndices[gridData.x + t]], worldPos, N);
        }
    }
    else if (LightCullingMode == LIGHT_CULLING_CLUSTER)
    {
        uint clusterIdx = ComputeClusterIndex(screenPos, worldPos);
        uint2 gridData = g_ClusterLightGrid[clusterIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcLightDiffuse(AllLights[g_ClusterLightIndices[gridData.x + t]], worldPos, N);
        }
    }
    else
    {
        for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        {
            result += CalcLightDiffuse(AllLights[i], worldPos, N);
        }
    }
}

void AccumulatePointSpotSpecular(float3 worldPos, float3 N, float3 V, float shininess, float4 screenPos, inout float3 result)
{
    if (LightCullingMode == LIGHT_CULLING_TILE && NumTilesX > 0 && NumTilesY > 0)
    {
        uint2 tileCoord = min(uint2(screenPos.xy) / TILE_SIZE, uint2(NumTilesX - 1, NumTilesY - 1));
        uint tileIdx = tileCoord.y * NumTilesX + tileCoord.x;
        uint2 gridData = TileLightGrid[tileIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcLightSpecular(AllLights[TileLightIndices[gridData.x + t]], worldPos, N, V, shininess);
        }
    }
    else if (LightCullingMode == LIGHT_CULLING_CLUSTER)
    {
        uint clusterIdx = ComputeClusterIndex(screenPos, worldPos);
        uint2 gridData = g_ClusterLightGrid[clusterIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcLightSpecular(AllLights[g_ClusterLightIndices[gridData.x + t]], worldPos, N, V, shininess);
        }
    }
    else
    {
        for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        {
            result += CalcLightSpecular(AllLights[i], worldPos, N, V, shininess);
        }
    }
}

#if defined(LIGHTING_MODEL_TOON) && LIGHTING_MODEL_TOON
static const float g_ToonSteps = 4.0f;
static const float g_ToonDarknessFloor = 0.25f;
static const float g_ToonRimMin = 0.55f;
static const float g_ToonRimMax = 0.85f;
static const float g_ToonRimStrength = 0.25f;

float ToonStep(float NdotL)
{
    float x = saturate(NdotL);
    float stepped = smoothstep(g_ToonDarknessFloor, 1.0f, x * g_ToonSteps);
    stepped /= max(g_ToonSteps - 1.0f, 1.0f);
    return lerp(g_ToonDarknessFloor, 1.0f, saturate(stepped));
}

float3 CalcToonDirectionalDiffuse(float3 N)
{
    float band = ToonStep(saturate(dot(N, -DirectionalLight.Direction)));
    return DirectionalLight.Color.rgb * DirectionalLight.Intensity * band;
}

float3 CalcToonPointSpotDiffuse(FLightInfo light, float3 worldPos, float3 N)
{
    float3 L = light.Position - worldPos;
    float dist = length(L);
    L = normalize(L);

    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);
    float band = ToonStep(saturate(dot(N, L)));

    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    return light.Color.rgb * light.Intensity * atten * spotFactor * band;
}

void AccumulateToonPointSpotDiffuse(float3 worldPos, float3 N, float4 screenPos, inout float3 result)
{
    if (LightCullingMode == LIGHT_CULLING_TILE && NumTilesX > 0 && NumTilesY > 0)
    {
        uint2 tileCoord = min(uint2(screenPos.xy) / TILE_SIZE, uint2(NumTilesX - 1, NumTilesY - 1));
        uint tileIdx = tileCoord.y * NumTilesX + tileCoord.x;
        uint2 gridData = TileLightGrid[tileIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcToonPointSpotDiffuse(AllLights[TileLightIndices[gridData.x + t]], worldPos, N);
        }
    }
    else if (LightCullingMode == LIGHT_CULLING_CLUSTER)
    {
        uint clusterIdx = ComputeClusterIndex(screenPos, worldPos);
        uint2 gridData = g_ClusterLightGrid[clusterIdx];
        for (uint t = 0; t < gridData.y; ++t)
        {
            result += CalcToonPointSpotDiffuse(AllLights[g_ClusterLightIndices[gridData.x + t]], worldPos, N);
        }
    }
    else
    {
        for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        {
            result += CalcToonPointSpotDiffuse(AllLights[i], worldPos, N);
        }
    }
}

float3 AccumulateToonDiffuse(float3 worldPos, float3 N, float4 screenPos)
{
    float3 result = CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity) * 0.15f;
    result += CalcToonDirectionalDiffuse(N);
    AccumulateToonPointSpotDiffuse(worldPos, N, screenPos, result);
    return result;
}

float CalcRimMask(float3 N, float3 V)
{
    float rimDot = 1.0f - saturate(dot(N, V));
    return smoothstep(g_ToonRimMin, g_ToonRimMax, rimDot);
}
#endif

float3 AccumulateDiffuse(float3 worldPos, float3 N, float4 screenPos)
{
    float3 result = float3(0, 0, 0);
    result += CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity);
    result += CalcDirectionalDiffuse(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                     DirectionalLight.Intensity, N);
    AccumulatePointSpotDiffuse(worldPos, N, screenPos, result);
    return result;
}

float3 AccumulateSpecular(float3 worldPos, float3 N, float3 V, float shininess, float4 screenPos)
{
    float3 result = float3(0, 0, 0);
    result += CalcDirectionalSpecular(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                      DirectionalLight.Intensity, N, V, shininess);
    AccumulatePointSpotSpecular(worldPos, N, V, shininess, screenPos, result);
    return result;
}

#endif // FORWARD_LIGHTING_HLSLI
