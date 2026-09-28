#include "../Common.hlsl"

// GBuffer
Texture2D SceneColor : register(t0);
Texture2D SceneNormal : register(t1);
Texture2D SceneDepth : register(t2);
Texture2D SceneLightColor : register(t3);
Texture2D SceneWorldPos : register(t4);

SamplerState SampleState : register(s0);

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 pos;
    if (vertexID == 0)
        pos = float2(-1.0f, -1.0f);
    else if (vertexID == 1)
        pos = float2(-1.0f, 3.0f);
    else
        pos = float2(3.0f, -1.0f);
    output.ClipPos = float4(pos, 0.0f, 1.0f);
    return output;
}

float ComputeFogTransmittance(FogLayerData fog, float rawDepth, float worldZ, float cameraDistance)
{
    if (fog.FogDensity <= 0.0f)
        return 1.0f;

    if (cameraDistance >= fog.FogCutoffDistance)
        return 1.0f;

    float scaledDensity = fog.FogDensity * 0.1f;
    float heightFactor = exp(-fog.HeightFalloff * max(worldZ - fog.FogHeight, 0.0f));
    float travelDistance = (rawDepth >= 1.0f) ? 1000.0f : max(cameraDistance - fog.FogStartDistance, 0.0f);

    float layerTransmittance = saturate(exp(-scaledDensity * heightFactor * travelDistance));
    float layerOpacity = min(saturate(1.0f - layerTransmittance), fog.FogMaxOpacity);
    return saturate(1.0f - layerOpacity);
}

float4 mainPS(VSOutput input) : SV_TARGET
{
    int2 ip = int2(input.ClipPos.xy);
    float rawDepth = SceneDepth.Load(int3(ip, 0)).r;
    float4 lightColor = SceneLightColor.Load(int3(ip, 0));
    float3 worldPos = SceneWorldPos.Load(int3(ip, 0)).xyz;
    float dist = length(worldPos - CameraPosition.xyz);

    const float MinTransmittance = 1e-4f;
    float totalOpticalDepth = 0.0f;
    float3 weightedFogColor = float3(0.0f, 0.0f, 0.0f);

    uint activeFogCount = min(FogLayerCount, (uint)MAX_FOG_LAYER_COUNT);
    [loop]
    for (uint fogIndex = 0; fogIndex < activeFogCount; ++fogIndex)
    {
        FogLayerData fog = FogLayers[fogIndex];
        float layerTransmittance = ComputeFogTransmittance(fog, rawDepth, worldPos.z, dist);
        if (layerTransmittance >= 1.0f)
            continue;

        float layerOpticalDepth = -log(max(layerTransmittance, MinTransmittance));
        totalOpticalDepth += layerOpticalDepth;
        weightedFogColor += fog.FogColor.rgb * layerOpticalDepth;
    }

    if (totalOpticalDepth <= 0.0f)
        return lightColor;

    float totalTransmittance = exp(-totalOpticalDepth);
    float3 fogColor = weightedFogColor / totalOpticalDepth;
    float3 outRgb = lightColor.rgb * totalTransmittance + fogColor * (1.0f - totalTransmittance);
    return float4(outRgb, lightColor.a);
}
