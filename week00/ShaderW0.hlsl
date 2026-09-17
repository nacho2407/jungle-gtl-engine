cbuffer constants: register(b0)
{
    float3 Offset;
    float Radius;
}

struct VS_INPUT
{
    float3 position: POSITION;
    float4 color: COLOR;
};

struct PS_INPUT
{
    float4 position: SV_POSITION;
    float4 color: COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = float4(input.position * Radius + Offset, 1.0f);
    output.color = input.color;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
