#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"

static const float PI = 3.14159265359f;

cbuffer DepthOfFieldCB : register(b2)
{
    // xy = full-resolution source texel size, zw = DOF target texel size
    float4 TexelSize;
    // x = focus distance, y = CoC scale, z = max foreground radius, w = max background radius
    float4 FocusParams;
    // x = near clip, y = far clip, z = target/source radius scale, w = gather sharpness
    float4 DepthParams;
    uint GatherRingCount;
    uint GatherSamplesPerRing;
    uint bEnableForeground;
    uint bEnableBackground;
    uint DebugView;
    uint bHalfRes;
    float SlightFocusRadius;
    float _Pad;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float LinearizeSceneDepth(float rawDepth)
{
    float nearZ = max(DepthParams.x, 0.001f);
    float farZ = max(DepthParams.y, nearZ + 0.001f);
    return nearZ * farZ / max(nearZ - rawDepth * (nearZ - farZ), 0.0001f);
}

float LoadLinearDepth(float2 uv)
{
    uint width;
    uint height;
    SceneDepthTexture.GetDimensions(width, height);

    int2 coord = int2(saturate(uv) * float2(width, height));
    coord = clamp(coord, int2(0, 0), int2(int(width) - 1, int(height) - 1));
    float rawDepth = SceneDepthTexture.Load(int3(coord, 0));
    return LinearizeSceneDepth(rawDepth);
}

float ComputeSignedCocFull(float linearDepth)
{
    float depth = max(linearDepth, 0.001f);
    float coc = ((depth - FocusParams.x) / depth) * FocusParams.y;
    return clamp(coc, -FocusParams.z, FocusParams.w);
}

float ComputeSignedCocTarget(float linearDepth)
{
    return ComputeSignedCocFull(linearDepth) * DepthParams.z;
}

float4 PS_Setup(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;
    float linearDepth = LoadLinearDepth(uv);
    float cocRadius = ComputeSignedCocTarget(linearDepth);
    return float4(sceneColor, cocRadius);
}

struct FGatherLayer
{
    float3 ColorSum;
    float WeightSum;
    float Coverage;
};

void AccumulateGatherSample(float4 sampleValue, float sampleDistance, bool bForeground, inout FGatherLayer layer)
{
    float coc = sampleValue.a;
    if (bForeground)
    {
        if (bEnableForeground == 0 || coc >= -0.0001f)
        {
            return;
        }
        coc = -coc;
    }
    else
    {
        if (bEnableBackground == 0 || coc <= 0.0001f)
        {
            return;
        }
    }

    float radiusMask = saturate(coc / 0.5f);
    float intersection = saturate((coc - sampleDistance) * DepthParams.w + 0.5f) * radiusMask;
    if (intersection <= 0.0001f)
    {
        return;
    }

    layer.ColorSum += sampleValue.rgb * intersection;
    layer.WeightSum += intersection;
    layer.Coverage = max(layer.Coverage, intersection);
}

float4 ResolveGatherLayer(FGatherLayer layer, float3 fallbackColor)
{
    float3 color = layer.WeightSum > 0.0001f ? layer.ColorSum / layer.WeightSum : fallbackColor;
    return float4(color, saturate(layer.Coverage));
}

struct FGatherOutput
{
    float4 Background : SV_Target0;
    float4 Foreground : SV_Target1;
};

FGatherOutput PS_Gather(PS_Input_UV input)
{
    float2 uv = input.uv;
    float4 center = DofSetupTexture.SampleLevel(LinearClampSampler, uv, 0);

    FGatherLayer background;
    background.ColorSum = 0.0f;
    background.WeightSum = 0.0f;
    background.Coverage = 0.0f;

    FGatherLayer foreground;
    foreground.ColorSum = 0.0f;
    foreground.WeightSum = 0.0f;
    foreground.Coverage = 0.0f;

    AccumulateGatherSample(center, 0.0f, false, background);
    AccumulateGatherSample(center, 0.0f, true, foreground);

    uint ringCount = clamp(GatherRingCount, 1u, 5u);
    uint samplesPerRing = clamp(GatherSamplesPerRing, 4u, 16u);
    float maxRadius = max(FocusParams.z, FocusParams.w) * DepthParams.z;

    [loop]
    for (uint ring = 1u; ring <= ringCount; ++ring)
    {
        float ringT = (float) ring / (float) ringCount;
        float radius = maxRadius * ringT;
        float angleJitter = ((ring & 1u) != 0u) ? 0.5f : 0.0f;

        [loop]
        for (uint sampleIndex = 0u; sampleIndex < samplesPerRing; ++sampleIndex)
        {
            float angle = (2.0f * PI) * ((float(sampleIndex) + angleJitter) / float(samplesPerRing));
            float2 offsetPixels = float2(cos(angle), sin(angle)) * radius;
            float2 sampleUV = uv + offsetPixels * TexelSize.zw;
            float4 sampleValue = DofSetupTexture.SampleLevel(LinearClampSampler, sampleUV, 0);
            float sampleDistance = length(offsetPixels);

            AccumulateGatherSample(sampleValue, sampleDistance, false, background);
            AccumulateGatherSample(sampleValue, sampleDistance, true, foreground);
        }
    }

    FGatherOutput output;
    output.Background = ResolveGatherLayer(background, center.rgb);
    output.Foreground = ResolveGatherLayer(foreground, center.rgb);
    return output;
}

float3 VisualizeCoc(float coc)
{
    float focusBand = 0.05f;
    if (abs(coc) <= focusBand)
    {
        return float3(0.03f, 0.03f, 0.03f);
    }

    if (coc < 0.0f)
    {
        float amount = saturate((-coc) / max(FocusParams.z, 0.001f));
        return lerp(float3(0.03f, 0.03f, 0.03f), float3(1.0f, 0.28f, 0.08f), amount);
    }
    else
    {
        float amount = saturate(coc / max(FocusParams.w, 0.001f));
        return lerp(float3(0.03f, 0.03f, 0.03f), float3(0.08f, 0.45f, 1.0f), amount);
    }
}

float4 PS_Recombine(PS_Input_UV input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 scene = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float linearDepth = LoadLinearDepth(uv);
    float cocFull = ComputeSignedCocFull(linearDepth);

    float4 background = DofBackgroundTexture.SampleLevel(LinearClampSampler, uv, 0);
    float4 foreground = DofForegroundTexture.SampleLevel(LinearClampSampler, uv, 0);

    if (DebugView == 1u)
    {
        return float4(VisualizeCoc(cocFull), 1.0f);
    }
    if (DebugView == 2u)
    {
        return float4(foreground.rgb * max(foreground.a, 0.15f), 1.0f);
    }
    if (DebugView == 3u)
    {
        return float4(background.rgb * max(background.a, 0.15f), 1.0f);
    }

    float3 result = scene.rgb;

    float backgroundBlend = 0.0f;
    if (bEnableBackground != 0u)
    {
        backgroundBlend = smoothstep(SlightFocusRadius, max(FocusParams.w, SlightFocusRadius + 0.001f), cocFull);
        backgroundBlend *= background.a;
    }

    float foregroundBlend = bEnableForeground != 0u ? foreground.a : 0.0f;
    foregroundBlend = saturate(foregroundBlend);

    result = lerp(result, background.rgb, saturate(backgroundBlend));
    result = lerp(result, foreground.rgb, foregroundBlend);

    return float4(result, scene.a);
}
