Texture2D ShaderTexture : register(t0);
SamplerState SampleType : register(s0);

cbuffer constants: register(b0)
{
    float3 Offset;
    float3 Scale;
    float3 CosSin;
    float UseTexture;
    float3 TintColor;
}

struct VS_INPUT
{
    float3 position: POSITION;
    float4 color: COLOR;
    float2 tex : TEXCOORD;
};

struct PS_INPUT
{
    float4 position: SV_POSITION;
    float4 color: COLOR;
    float2 tex : TEXCOORD;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
     // 1. 스케일
    float3 scaled = input.position * Scale;

    float3 rotated;
     // 2. 회전 (스케일된 값에 적용)
    rotated.x = scaled.x * CosSin.x - scaled.y * CosSin.y;
    rotated.y = scaled.x * CosSin.y + scaled.y * CosSin.x;
    rotated.z = scaled.z;

     // 3. 이동
    output.position = float4(rotated + Offset, 1.0f);
    output.color = input.color;
    output.tex = input.tex;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    if (UseTexture == 0.0f)
    {
        //return input.color;
    }
    float4 textureColor;
    textureColor = ShaderTexture.Sample(SampleType, input.tex);

    textureColor.rgb *= TintColor;
    
    return textureColor;
}
