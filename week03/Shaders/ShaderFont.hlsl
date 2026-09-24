cbuffer FrameConstant : register(b0)
{
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer FontConstant : register(b1)
{
    float3 CameraRight;
    float padding0; // DX 11의 상수 버퍼는 데이터가 16바이트 단위를 맞추어야 함
    
    float3 CameraUp;
    float padding1;
    
    float4 TextColor;
}

struct VS_INPUT
{
    float3 Anchor : POSITION;
    float2 Offset : TEXCOORD0;
    float2 UV : TEXCOORD1;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD;
};

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    float3 WorldPos = Input.Anchor;
    
    // billboard offset 적용
    WorldPos += CameraRight * Input.Offset.x;
    WorldPos += CameraUp * Input.Offset.y;
    
    Output.Pos = mul(float4(WorldPos, 1.0f), mul(View, Projection));
    Output.UV = Input.UV;

    return Output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float mask = FontAtlas.Sample(FontSampler, input.UV).r;

    return float4(TextColor.rgb, TextColor.a * mask);
}