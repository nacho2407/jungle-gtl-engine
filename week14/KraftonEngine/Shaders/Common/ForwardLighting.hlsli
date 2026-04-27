#ifndef FORWARD_LIGHTING_HLSLI
#define FORWARD_LIGHTING_HLSLI

#include "Common/ForwardLightData.hlsli"
#include "Common/ShadowSampling.hlsli"

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
    float slice;
    
    if (CullState.bIsOrtho > 0)
    {
        slice = (safeDepth - CullState.NearZ) / (CullState.FarZ - CullState.NearZ);
    }
    else
    {
        slice = log(safeDepth / CullState.NearZ) / log(CullState.FarZ / CullState.NearZ);
    }
    
    return min((uint) floor(slice * CullState.ClusterZ), CullState.ClusterZ - 1);
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

// Culling 모드(Tile/Cluster/None)에 따라 현재 픽셀의 라이트 수를 조회하고 heatmap 색상 반환
float4 ComputeCullingHeatmap(float4 screenPos, float3 worldPos)
{
    uint LightCount = NumActivePointLights + NumActiveSpotLights;

    if (LightCullingMode == LIGHT_CULLING_TILE && NumTilesX > 0 && NumTilesY > 0)
    {
        uint2 tileCoord = min(uint2(screenPos.xy) / TILE_SIZE, uint2(NumTilesX - 1, NumTilesY - 1));
        uint tileIdx = tileCoord.y * NumTilesX + tileCoord.x;
        LightCount = TileLightGrid[tileIdx].y;
    }
    else if (LightCullingMode == LIGHT_CULLING_CLUSTER)
    {
        uint clusterIdx = ComputeClusterIndex(screenPos, worldPos);
        LightCount = g_ClusterLightGrid[clusterIdx].y;
    }

    float ratio = saturate((float)LightCount / HeatMapMax);
    return float4(GetHeatmapColor(ratio), 1.0f);
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

    float shadow = 1.0f;
    if (light.bCastShadow)
    {
        if (light.LightType == LIGHT_TYPE_SPOT)
            shadow = CalcSpotShadowFactor(light.ShadowMapIndex, worldPos);
        else if (light.LightType == LIGHT_TYPE_POINT)
            shadow = CalcPointShadowFactor(light.ShadowMapIndex, worldPos, light.Position);
    }

    return light.Color.rgb * light.Intensity * NdotL * atten * spotFactor * shadow;
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

    float shadow = 1.0f;
    if (light.bCastShadow)
    {
        if (light.LightType == LIGHT_TYPE_SPOT)
            shadow = CalcSpotShadowFactor(light.ShadowMapIndex, worldPos);
        else if (light.LightType == LIGHT_TYPE_POINT)
            shadow = CalcPointShadowFactor(light.ShadowMapIndex, worldPos, light.Position);
    }

    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    return light.Color.rgb * light.Intensity * pow(NdotH, max(shininess, 1.0f)) * atten * spotFactor * shadow;
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

float CalcDirectionalShadow(float3 worldPos)
{
    float viewDepth = abs(mul(float4(worldPos, 1.0f), View).z);
    return CalcDirectionalShadowFactor(worldPos, viewDepth);
}

float3 AccumulateDiffuse(float3 worldPos, float3 N, float4 screenPos)
{
    float3 result = float3(0, 0, 0);
    result += CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity);

    float3 dirDiffuse = CalcDirectionalDiffuse(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                               DirectionalLight.Intensity, N);
    float viewDepth = abs(mul(float4(worldPos, 1.0f), View).z);
    dirDiffuse *= CalcDirectionalShadowFactor(worldPos, viewDepth);
    result += dirDiffuse;

    AccumulatePointSpotDiffuse(worldPos, N, screenPos, result);
    return result;
}

float3 AccumulateSpecular(float3 worldPos, float3 N, float3 V, float shininess, float4 screenPos)
{
    float3 result = float3(0, 0, 0);

    float3 dirSpec = CalcDirectionalSpecular(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                             DirectionalLight.Intensity, N, V, shininess);
    float viewDepth = abs(mul(float4(worldPos, 1.0f), View).z);
    dirSpec *= CalcDirectionalShadowFactor(worldPos, viewDepth);
    result += dirSpec;

    AccumulatePointSpotSpecular(worldPos, N, V, shininess, screenPos, result);
    return result;
}

// ── Vertex Shader 전용 (Culling 우회) ──
float3 AccumulateDiffuseVS(float3 worldPos, float3 N)
{
    float3 result = CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity);
    result += CalcDirectionalDiffuse(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                     DirectionalLight.Intensity, N);
    //light culling에서는 픽셀 좌표(0~1920)를 기대하지만, 버텍스 셰이더(VS)의 좌표는 클립 공간(-1~1).
    //아주 작은 좌표값을 사용하여 잘못된 타일 인덱스를 참조했고 조명 목록을 가져오지 못해 빛이 사라짐.
    // 전체 라이트 순회로 해결(임시로 VS는 정점 수가 적으므로 성능을 일부 포기하고 버그를 해결.)
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
    {
        result += CalcLightDiffuse(AllLights[i], worldPos, N);
    }
    return result;
}

float3 AccumulateSpecularVS(float3 worldPos, float3 N, float3 V, float shininess)
{
    float3 result = CalcDirectionalSpecular(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                      DirectionalLight.Intensity, N, V, shininess);
    
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
    {
        result += CalcLightSpecular(AllLights[i], worldPos, N, V, shininess);
    }
    return result;
}

#endif // FORWARD_LIGHTING_HLSLI
