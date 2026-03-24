cbuffer FObjectIdConstants : register(b0)
{
    row_major float4x4 MVP;
    uint ObjectId;
    uint Padding0;
    uint Padding1;
    uint Padding2;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VSMain(VSInput In)
{
    VSOutput Out;
    Out.Position = mul(float4(In.Position, 1.0f), MVP);
    return Out;
}

uint PSMain(VSOutput In) : SV_TARGET
{
    return ObjectId;
}