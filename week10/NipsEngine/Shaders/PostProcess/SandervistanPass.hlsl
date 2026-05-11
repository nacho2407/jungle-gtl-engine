Texture2D SceneTex : register(t0);
SamplerState Samp : register(s0);

cbuffer SandevistanCB : register(b11)
{
    float Time;
    float Intensity;
    float Padding0;
    float Padding1;

    float2 Center;
    float2 InvResolution;
}

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

float2 Distort(float2 uv)
{
    float2 dir = uv - Center;
    float dist = length(dir);

    float strength = dist * Intensity * 0.5;

    return uv + dir * strength;
}

float3 Chromatic(float2 uv)
{
    float2 offset = (uv - Center) * 0.01 * Intensity;

    float r = SceneTex.Sample(Samp, uv + offset).r;
    float g = SceneTex.Sample(Samp, uv).g;
    float b = SceneTex.Sample(Samp, uv - offset).b;

    return float3(r, g, b);
}

float3 MotionBlur(float2 uv)
{
    float2 dir = normalize(uv - Center);

    float3 col = 0;
    float weightSum = 0;

    [unroll]
    for (int i = 0; i < 5; i++)
    {
        float t = i / 5.0;
        float2 sampleUV = uv - dir * t * 0.05 * Intensity;

        float w = 1.0 - t;
        col += SceneTex.Sample(Samp, sampleUV).rgb * w;
        weightSum += w;
    }

    return col / weightSum;
}

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

    float2 distortedUV = Distort(uv);
    distortedUV = saturate(distortedUV);

    float3 base = MotionBlur(distortedUV);
    float3 chroma = Chromatic(distortedUV);

    float3 finalColor = lerp(base, chroma, 0.6);

    return float4(finalColor, 1);
}