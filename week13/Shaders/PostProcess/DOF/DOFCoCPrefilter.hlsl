#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"
#include "Common/DepthOfField.hlsli"

Texture2D<float4> DOFColorCoCTex : register(t0);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_Target
{
    float2 uv = input.uv;

    float nearCoC = 0.0f;
    float farCoC = 0.0f;
    float3 nearColor = 0.0f;
    float3 farColor = 0.0f;
    float nearWeightSum = 0.0f;
    float farWeightSum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y);
            float spatialWeight = (x == 0 && y == 0) ? 1.0f : ((x == 0 || y == 0) ? 0.65f : 0.35f);
            float4 sampleValue = DOFColorCoCTex.SampleLevel(LinearClampSampler, uv + offset * DOFInvHalfResolution, 0);

            float sampleNear = max(-sampleValue.a, 0.0f);
            float sampleFar = max(sampleValue.a, 0.0f);
            nearCoC = max(nearCoC, sampleNear * spatialWeight);
            farCoC = max(farCoC, sampleFar * spatialWeight);

            float nearWeight = sampleNear * spatialWeight;
            float farWeight = sampleFar * spatialWeight;
            nearColor += sampleValue.rgb * nearWeight;
            farColor += sampleValue.rgb * farWeight;
            nearWeightSum += nearWeight;
            farWeightSum += farWeight;
        }
    }

    float4 center = DOFColorCoCTex.SampleLevel(LinearClampSampler, uv, 0);
    float signedCoC = (nearCoC > farCoC * 0.85f) ? -nearCoC : farCoC;
    float3 filteredColor = center.rgb;

    if (signedCoC < 0.0f && nearWeightSum > 0.0001f)
    {
        filteredColor = lerp(center.rgb, nearColor / nearWeightSum, 0.65f);
    }
    else if (signedCoC > 0.0f && farWeightSum > 0.0001f)
    {
        filteredColor = lerp(center.rgb, farColor / farWeightSum, 0.45f);
    }

    return float4(filteredColor, signedCoC);
}
