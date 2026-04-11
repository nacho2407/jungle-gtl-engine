#include "Common.hlsl"

cbuffer DecalBuffer : register(b8)
{
    row_major matrix InvDecalWorld;
    float4 DecalColorTint;
}

Texture2D DiffuseMap : register(t0);
SamplerState SampleState : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
};

PSInput mainVS(VSInput input)
{
    PSInput output;
    output.WorldPos = mul(float4(input.Position, 1.0f), Model).xyz;

    float3x3 normalMatrix = transpose(Inverse3x3((float3x3)Model));
    output.WorldNormal = normalize(mul(input.Normal, normalMatrix));
    output.ClipPos = ApplyMVP(input.Position);
    output.ClipPos.z -= 0.1f;
    output.UV = input.UV;
    return output;
}

PSOutput mainPS(PSInput input) : SV_Target
{
    PSOutput output;
    
    float4 localPos = mul(float4(input.WorldPos, 1.0f), InvDecalWorld);

    clip(0.5f - abs(localPos.xyz));
    
    float2 decalUV;
    decalUV.x = localPos.y + 0.5f;
    decalUV.y = localPos.z + 0.5f;
    decalUV.y = 1.0f - decalUV.y;

    float4 decalTex = DiffuseMap.Sample(SampleState, decalUV);
    
    float3 finalColor = decalTex.rgb * DecalColorTint.rgb;
    float finalAlpha = decalTex.a * DecalColorTint.a;
    
    output.Color = float4(finalColor, finalAlpha);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
}
