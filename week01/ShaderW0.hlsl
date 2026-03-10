cbuffer constants: register(b0)
{
    float3 Offset;
    float  Radius;
    
    float3 CosSin;
    float Padding;
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
    
     // 1. 스케일
    float3 scaled = input.position * Radius;

    float3 rotated;
     // 2. 회전 (스케일된 값에 적용)
    rotated.x = scaled.x * CosSin.x - scaled.y * CosSin.y;
    rotated.y = scaled.x * CosSin.y + scaled.y * CosSin.x;
    rotated.z = scaled.z;

     // 3. 이동
    output.position = float4(rotated + Offset, 1.0f);
    output.color = input.color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
