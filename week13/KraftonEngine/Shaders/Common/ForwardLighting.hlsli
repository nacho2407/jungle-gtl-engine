#ifndef FORWARD_LIGHTING_HLSLI
#define FORWARD_LIGHTING_HLSLI

// =============================================================================
// ForwardLighting.hlsli — 공용 Forward Lighting 유틸 함수
// UberLit, DecalShader 등 라이팅이 필요한 모든 셰이더에서 include
// =============================================================================

#include "Common/ForwardLightData.hlsli"

// =============================================================================
// 기본 라이팅 유틸 함수
// =============================================================================

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

// =============================================================================
// FLightInfo 기반 계산 (Point/Spot 공용)
// =============================================================================

float3 CalcLightDiffuse(FLightInfo light, float3 worldPos, float3 N)
{
    float3 L    = light.Position - worldPos;
    float  dist = length(L);
    L = normalize(L);

    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);
    float NdotL = saturate(dot(N, L));

    // Spot cone falloff (Point는 InnerConeCos=-1, OuterConeCos=-1 → spotFactor=1)
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
    float3 L    = normalize(light.Position - worldPos);
    float  dist = length(light.Position - worldPos);
    float  atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);

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

// =============================================================================
// 통합 라이팅 누적
// =============================================================================

float3 AccumulateDiffuse(float3 worldPos, float3 N, float4 screenPos)
{
    float3 result = float3(0, 0, 0);

    // Ambient + Directional (CB b4)
    result += CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity);
    result += CalcDirectionalDiffuse(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                     DirectionalLight.Intensity, N);

    // Point/Spot (StructuredBuffer t8, Tile Culling 적용 시 t9/t10 경유)
    #if defined(USE_TILE_CULLING) && USE_TILE_CULLING
    uint2 tileCoord = uint2(screenPos.xy) / 16;
    uint tileIdx    = tileCoord.y * NumTilesX + tileCoord.x;
    uint2 gridData  = TileLightGrid[tileIdx];
    for (uint t = 0; t < gridData.y; ++t)
    {
        uint lightIdx = TileLightIndices[gridData.x + t];
        result += CalcLightDiffuse(AllLights[lightIdx], worldPos, N);
    }
    #else
    // 전수 순회 (Culling 미적용)
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightDiffuse(AllLights[i], worldPos, N);
    #endif

    return result;
}

float3 AccumulateSpecular(float3 worldPos, float3 N, float3 V, float shininess, float4 screenPos)
{
    float3 result = float3(0, 0, 0);

    result += CalcDirectionalSpecular(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                      DirectionalLight.Intensity, N, V, shininess);
    
    #if defined(USE_TILE_CULLING) && USE_TILE_CULLING
    uint2 tileCoord = uint2(screenPos.xy) / 16;
    uint tileIdx = tileCoord.y * NumTilesX + tileCoord.x;
    uint2 gridData = TileLightGrid[tileIdx];
    uint tileLightCount = gridData.y;
    
    for (uint t = 0; t < gridData.y; ++t)
    {
        uint lightIdx = TileLightIndices[gridData.x + t];
        result += CalcLightSpecular(AllLights[lightIdx], worldPos, N, V, shininess);
    }
    #else
    uint tileLightCount = NumActivePointLights + NumActiveSpotLights;
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightSpecular(AllLights[i], worldPos, N, V, shininess);
    #endif
    
    return result;
}

#endif // FORWARD_LIGHTING_HLSLI
