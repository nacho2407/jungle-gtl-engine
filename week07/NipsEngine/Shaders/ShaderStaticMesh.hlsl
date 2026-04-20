#include "Common.hlsl"
#include "UberLit.hlsl"

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
    float3 VertexLighting : TEXCOORD3;
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
    
#ifdef LIGHTING_MODEL_GOURAUD
    FUberMaterialInput MaterialInput;
    MaterialInput.Albedo = float3(1.0f, 1.0f, 1.0f);   // 조명 factor만 계산
    MaterialInput.Emissive = float3(0.0f, 0.0f, 0.0f);
    MaterialInput.SpecularColor = SpecularColor;
    MaterialInput.Shininess = Shininess;

    FUberLightingInput LightingInput;
    LightingInput.WorldPos = output.WorldPos;
    LightingInput.WorldNormal = output.WorldNormal;
    LightingInput.ViewDir = normalize(CameraPosition - output.WorldPos);

    output.VertexLighting = EvaluateUberLighting(MaterialInput, LightingInput).Color;
#endif
    
    return output;
}

PSOutput mainPS(PSInput input)
{
    PSOutput output;

    float4 DiffuseTex = float4(1.f, 1.f, 1.f, 1.f);
    if ((bool) bHasDiffuseMap)
    {
        DiffuseTex = DiffuseMap.Sample(SampleState, input.UV);
        clip(DiffuseTex.a - 0.001f);
    }

    float3 Albedo = DiffuseColor * DiffuseTex.rgb;
    float3 FinalColor = Albedo;

#ifndef LIGHTING_MODEL_GOURAUD
    FUberMaterialInput MaterialInput;
    MaterialInput.Albedo = Albedo;
    MaterialInput.Emissive = EmissiveColor;
    MaterialInput.SpecularColor = SpecularColor;
    MaterialInput.Shininess = Shininess;

    FUberLightingInput LightingInput;
    LightingInput.WorldPos = input.WorldPos;
    LightingInput.WorldNormal = input.WorldNormal;
    LightingInput.ViewDir = normalize(CameraPosition - input.WorldPos);

    FinalColor = EvaluateUberLighting(MaterialInput, LightingInput).Color;
#else
    FinalColor = Albedo * input.VertexLighting + EmissiveColor;
#endif

    if (bIsWireframe > 0.5f)
    {
        FinalColor = WireframeRGB;
    }

    output.Color = float4(FinalColor, 1.f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
}
