#include "Common/Functions.hlsli"
#include "Common/SystemResources.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFFarBlurTex : register(t0);
Texture2D<float4> DOFNearBlurTex : register(t1);
Texture2D<float4> DOFBokehTex : register(t2);

float4 SampleFarBlurBilateral(float2 uv, float signedCoC)
{
    float target = saturate(max(signedCoC, 0.0f) / max(DOFMaxCoCRadius, 0.001f));
    float3 accum = 0.0f;
    float alphaAccum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUV = uv + float2((float)x, (float)y) * DOFInvHalfResolution;
            float4 sampleValue = DOFFarBlurTex.SampleLevel(LinearClampSampler, sampleUV, 0);
            float sampleDepth = SceneDepthTexture.SampleLevel(PointClampSampler, sampleUV, 0).r;
            float sampleCoC = CalculateSignedCoC(sampleDepth);
            float sampleTarget = saturate(max(sampleCoC, 0.0f) / max(DOFMaxCoCRadius, 0.001f));
            float cocWeight = 1.0f / (1.0f + abs(sampleTarget - target) * 8.0f);
            float sideWeight = (sampleCoC >= -0.01f) ? 1.0f : 0.05f;
            float spatialWeight = (x == 0 && y == 0) ? 1.0f : ((x == 0 || y == 0) ? 0.65f : 0.35f);
            float weight = spatialWeight * cocWeight * sideWeight * max(sampleValue.a, 0.05f);

            accum += sampleValue.rgb * weight;
            alphaAccum += sampleValue.a * weight;
            weightSum += weight;
        }
    }

    if (weightSum <= 0.0001f)
    {
        return DOFFarBlurTex.SampleLevel(LinearClampSampler, uv, 0);
    }

    return float4(accum / weightSum, saturate(alphaAccum / weightSum));
}

float4 SampleNearBlurBilateral(float2 uv, float signedCoC)
{
    float target = saturate(max(-signedCoC, 0.0f) / max(DOFMaxCoCRadius, 0.001f));
    float3 accum = 0.0f;
    float alphaAccum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUV = uv + float2((float)x, (float)y) * DOFInvHalfResolution;
            float4 sampleValue = DOFNearBlurTex.SampleLevel(LinearClampSampler, sampleUV, 0);
            float sampleDepth = SceneDepthTexture.SampleLevel(PointClampSampler, sampleUV, 0).r;
            float sampleCoC = CalculateSignedCoC(sampleDepth);
            float sampleTarget = saturate(max(-sampleCoC, 0.0f) / max(DOFMaxCoCRadius, 0.001f));
            float cocWeight = 1.0f / (1.0f + abs(sampleTarget - target) * 6.0f);
            float spatialWeight = (x == 0 && y == 0) ? 1.0f : ((x == 0 || y == 0) ? 0.7f : 0.4f);
            float weight = spatialWeight * lerp(0.5f, 1.0f, cocWeight) * max(sampleValue.a, 0.001f);

            accum += sampleValue.rgb * weight;
            alphaAccum += sampleValue.a * weight;
            weightSum += weight;
        }
    }

    if (weightSum <= 0.0001f)
    {
        return DOFNearBlurTex.SampleLevel(LinearClampSampler, uv, 0);
    }

    return float4(accum / weightSum, saturate(alphaAccum / weightSum));
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;

    float4 sharpColor = SceneColorTexture.SampleLevel(LinearClampSampler, uv, 0);
    float depth = SceneDepthTexture.SampleLevel(PointClampSampler, uv, 0).r;
    float signedCoC = CalculateSignedCoC(depth);
    float4 farBlur = SampleFarBlurBilateral(uv, signedCoC);
    float4 nearBlur = SampleNearBlurBilateral(uv, signedCoC);
    float3 bokeh = DOFBokehTex.SampleLevel(LinearClampSampler, uv, 0).rgb;

    float farFactor = signedCoC > 0.0f ? saturate(signedCoC / max(DOFMaxCoCRadius, 0.001f)) : 0.0f;
    farFactor = smoothstep(0.0f, 1.0f, farFactor);
    farFactor = max(farFactor, farBlur.a);

    float3 color = lerp(sharpColor.rgb, farBlur.rgb, farFactor);
    color = lerp(color, nearBlur.rgb, nearBlur.a);
    color += bokeh;

    return float4(color, sharpColor.a);
}
