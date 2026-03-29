#include "Common.hlsl"

cbuffer StaticMeshBuffer : register(b6)
{
    float3 AmbientColor;    // Ka
    float  Padding6_1;
    float3 DiffuseColor;    // Kd
    float  Padding6_2;
    float3 SpecularColor;   // Ks
    float  Shininess;       // Ns

    // Camera
    float3 CameraWorldPos;
    float  Padding6_3;
    
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


PSInput mainVS(VSInput input)
{
    PSInput output;
    
    output.WorldPos = mul(float4(input.Position, 1.0f), Model);
    output.WorldNormal = input.Normal;
    output.UV = input.UV;
    output.ClipPos = ApplyMVP(input.Position);
    return output;
}


float4 mainPS(PSInput input) : SV_TARGET
{
    float4 Diffuse = DiffuseMap.Sample(SampleState, input.UV);
    
    return Diffuse;
}
