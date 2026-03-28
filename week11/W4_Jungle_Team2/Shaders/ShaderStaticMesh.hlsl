#include "Common.hlsl"

cbuffer StaticMeshBuffer : register(b6)
{
    float3 LightDir;    
    float  LightIntensity;
    float3 LightColor;
    float  Padding6_0;
    
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
    output.ClipPos = float4(0, 0, 0, 1);
    output.WorldPos = float3(0, 0, 0);
    output.WorldNormal = float3(0, 0, 0);
    output.UV = float2(0, 0);
    return output;
}


float4 mainPS(PSInput input) : SV_TARGET
{
    return float4(0.f, 0.f ,0.f, 1.f);
}
