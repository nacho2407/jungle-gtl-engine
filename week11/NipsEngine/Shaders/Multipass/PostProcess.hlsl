Texture2D SceneTex : register(t0);
SamplerState Sampler : register(s0);

cbuffer PostProcessCB : register(b11)
{
    float2 InvResolution;
    float VignetteIntensity;
    float VignetteRadius;
    float VignetteSmoothness;
    uint GammaCorrectionEnabled;
    float2 Padding;
    float4 VignetteColor;
}

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

VSOutput mainVS(uint id : SV_VertexID)
{
    float2 pos;
    if (id == 0)
        pos = float2(-1, -1);
    else if (id == 1)
        pos = float2(-1, 3);
    else
        pos = float2(3, -1);

    VSOutput o;
    o.Pos = float4(pos, 0, 1);
    return o;
}

float4 mainPS(VSOutput input) : SV_TARGET
{
    float2 uv = input.Pos.xy * InvResolution;

    float3 color = SceneTex.Sample(Sampler, uv).rgb;

    if (VignetteIntensity > 0.001)
    {
        float aspect = InvResolution.y / InvResolution.x;
        float2 centered = uv - 0.5;
        centered.x *= aspect;
        float distanceFromCenter = length(centered);
        float outer = VignetteRadius + max(VignetteSmoothness, 0.001);
        float vignette = smoothstep(VignetteRadius, outer, distanceFromCenter);
        color = lerp(color, VignetteColor.rgb, vignette * saturate(VignetteIntensity));
    }

    if (GammaCorrectionEnabled != 0)
    {
        color = pow(color, 1.0 / 2.2);
    }
    
    return float4(color, 1.0);
}
