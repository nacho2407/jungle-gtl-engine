// Hi-Z Debug Visualization — R32_FLOAT depth → RGBA grayscale
// Applies power curve to reveal depth detail (standard depth is clustered near 1.0)

cbuffer VisualizeParams : register(b0)
{
    float Exponent;     // pow(depth, Exponent) — higher = more contrast
    float NearClip;
    float FarClip;
    uint  Mode;         // 0 = raw power, 1 = linearized depth
};

Texture2D<float> InputTex : register(t0);
RWTexture2D<float4> OutputTex : register(u0);

[numthreads(8, 8, 1)]
void CSHiZVisualize(uint3 DTid : SV_DispatchThreadID)
{
    float d = InputTex.Load(int3(DTid.xy, 0));

    float v;
    if (Mode == 1)
    {
        // Linearize perspective depth → [0,1] over [near,far]
        float linZ = NearClip * FarClip / (FarClip - d * (FarClip - NearClip));
        v = saturate((linZ - NearClip) / (FarClip - NearClip));
    }
    else
    {
        v = pow(saturate(d), Exponent);
    }

    OutputTex[DTid.xy] = float4(v, v, v, 1.0);
}
