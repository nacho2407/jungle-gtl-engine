#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D BloomTexture : register(t26);

cbuffer BloomBlurCB : register(b2)
{
    float2 TexelSize; // 1.0 / texture width, 1.0 / texture height
    float2 Direction; // horizontal: float2(1, 0), vertical: float2(0, 1)
    float Radius;
    float3 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float2 stepUV = TexelSize * Direction * Radius;

    float3 color = 0.0f;

    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv - stepUV * 4.0f, 0).rgb * 0.016216f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv - stepUV * 3.0f, 0).rgb * 0.054054f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv - stepUV * 2.0f, 0).rgb * 0.121622f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv - stepUV * 1.0f, 0).rgb * 0.194594f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv, 0).rgb * 0.227027f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv + stepUV * 1.0f, 0).rgb * 0.194594f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv + stepUV * 2.0f, 0).rgb * 0.121622f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv + stepUV * 3.0f, 0).rgb * 0.054054f;
    color += BloomTexture.SampleLevel(LinearClampSampler, input.uv + stepUV * 4.0f, 0).rgb * 0.016216f;

    return float4(color, 1.0f);
}
