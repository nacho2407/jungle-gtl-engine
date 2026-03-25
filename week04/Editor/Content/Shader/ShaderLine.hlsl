cbuffer FLineConstants : register(b0)
{
    row_major float4x4 VP;
    float3 CameraPos;
    float MaxDistance;
};

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR0;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

PSInput VSMain(VSInput In)
{
    PSInput Out;
    Out.Position = mul(float4(In.Position, 1.0f), VP);
    
    float dist = distance(In.Position, CameraPos);
    float fadeFactor = saturate((MaxDistance - dist) / (MaxDistance));
    float alpha = fadeFactor * fadeFactor;
    Out.Color = In.Color;
    Out.Color.a *= (alpha * 0.8f);
    
    return Out;
}

float4 PSMain(PSInput In) : SV_TARGET
{
    return In.Color;
}
