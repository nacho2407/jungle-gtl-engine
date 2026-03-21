cbuffer FontTransform : register(b0)
{
    row_major float4x4 ViewProj;
    float3 WorldPos;  float _pad0;
    float3 CamRight;  float _pad1;
    float3 CamUp;     float _pad2;
};

Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PSInput FontVS(VSInput input)
{
    PSInput output;
    
    float3 worldPos = WorldPos + CamRight * input.position.x + CamUp * input.position.y;
    output.position = mul(float4(worldPos, 1.0f), ViewProj);
    output.texCoord = input.texCoord;
    return output;
}

float4 FontPS(PSInput input) : SV_TARGET
{
    float4 col = FontAtlas.Sample(FontSampler, input.texCoord);
    if (col.r < 0.1f) discard; // 투명 픽셀 제거
    
    return col;
}