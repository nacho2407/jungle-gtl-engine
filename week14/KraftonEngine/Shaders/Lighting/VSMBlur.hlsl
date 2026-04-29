// =============================================================================
// VSMBlur.hlsl — Separable Gaussian Blur for VSM moment textures
// =============================================================================
// Fullscreen triangle PS. Direction (H/V) and array slice via b2 cbuffer.
// Input:  Texture2DArray (R32G32_FLOAT moments)  — t0
// Output: Blurred moments to RTV (same format)
//
// 2-pass: Horizontal → Temp texture, Vertical → Original texture
// =============================================================================

#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

cbuffer VSMBlurParams : register(b2)
{
    float2 TexelDir;    // (1/W, 0) for H,  (0, 1/H) for V
    float  ArraySlice;  // which slice to sample
    float  BlurRadius;  // kernel half-size in texels (0 = no blur)
};

Texture2DArray<float2> MomentTexture : register(t0);

// Fullscreen triangle VS (SV_VertexID)
PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    PS_Input_UV output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

// 7-tap Gaussian weights (sigma ~= 1.5, normalized)
// Supports dynamic radius: only sample up to BlurRadius taps
static const int   MAX_HALF_SIZE = 3;
static const float Weights[4] = { 0.266346, 0.215007, 0.113085, 0.038735 };
// Weights: G(0)=0.266346, G(1)=0.215007, G(2)=0.113085, G(3)=0.038735
// Sum(symmetric) = 0.266346 + 2*(0.215007+0.113085+0.038735) = 1.0

float2 PS(PS_Input_UV input) : SV_TARGET
{
    int halfSize = (int)clamp(BlurRadius, 0, MAX_HALF_SIZE);

    // Early out: no blur
    if (halfSize == 0)
    {
        return MomentTexture.SampleLevel(LinearClampSampler, float3(input.uv, ArraySlice), 0);
    }

    float2 result = float2(0, 0);
    float  totalWeight = 0;

    for (int i = -halfSize; i <= halfSize; ++i)
    {
        float2 offset = TexelDir * (float)i;
        float3 uvw = float3(input.uv + offset, ArraySlice);
        float  w = Weights[abs(i)];

        result += MomentTexture.SampleLevel(LinearClampSampler, uvw, 0) * w;
        totalWeight += w;
    }

    return result / totalWeight;
}
