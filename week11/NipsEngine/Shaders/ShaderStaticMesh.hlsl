#include "Common.hlsl"

cbuffer StaticMeshBuffer : register(b2)
{
    float3 AmbientColor;    // Ka
    float padding0;
    
    float3 DiffuseColor;    // Kd
    float padding1;
    
    float3 SpecularColor;   // Ks
    float Shininess; // Ns    
    
    float2 ScrollUV;
    uint   bHasDiffuseMap;
    uint   bHasSpecularMap;
    
    float3 EmissiveColor;    // emissive glow color; non-zero means emissive
    float padding2;
};

Texture2D DiffuseMap  : register(t0);
Texture2D AmbientMap  : register(t1);
Texture2D SpecularMap : register(t2);
Texture2D BumpMap     : register(t3);

SamplerState SampleState : register(s0);

struct VSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD;
};

struct PSInput
{
    float4 ClipPos     : SV_POSITION;
    float3 WorldPos    : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV          : TEXCOORD2;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 WorldPos : SV_TARGET2;
};


PSInput mainVS(VSInput input)
{
    PSInput output;

    output.WorldPos    = mul(float4(input.Position, 1.0f), Model).xyz;
    
    // 비균일 스케일을 위한 역행렬 이후 전치 
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.UV          = input.UV + ScrollUV;
    output.ClipPos     = ApplyMVP(input.Position);
    return output;
}


PSOutput mainPS(PSInput input) : SV_TARGET
{
    PSOutput output;
    
    float3 DiffuseTex = float3(1.f, 1.f, 1.f);
    if ((bool)bHasDiffuseMap)
    {
        DiffuseTex = DiffuseMap.Sample(SampleState, input.UV).xyz;
    }
    
    float3 FinalColor = DiffuseColor * DiffuseTex;
    
    if (any(EmissiveColor > 0.f))
    {
        // Emissive surface: write the glow color and mark normal.a = 2
        output.Color    = float4(EmissiveColor * DiffuseTex, 1.f);
        output.Normal   = float4(input.WorldNormal * 0.5f + 0.5f, 2.f);
        output.WorldPos = float4(input.WorldPos, 1.f);
        return output;
    }

    if (bIsWireframe > 0.5f)
    {
        FinalColor = WireframeRGB;
    }

    output.Color = float4(FinalColor, 1.f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
}
