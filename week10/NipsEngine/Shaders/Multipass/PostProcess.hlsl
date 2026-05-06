Texture2D SceneTex : register(t0);
SamplerState Sampler : register(s0);

cbuffer PostProcessCB : register(b11)
{
    float2 InvResolution;
    float2 Padding;
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

    // gamma correction
    color = pow(color, 1.0 / 2.2);
    
    return float4(color, 1.0);
}