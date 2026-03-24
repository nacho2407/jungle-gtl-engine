cbuffer PickingConstants : register(b0)
{
    row_major float4x4 MVP;
    uint ObjectId;
    float3 Padding;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OUTPUT mainVS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = mul(float4(input.position, 1.0f), MVP);
    return output;
}

uint mainPS(VS_OUTPUT input) : SV_TARGET
{
    return ObjectId;
}