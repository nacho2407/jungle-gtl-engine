#include "Common.hlsl"
#include "UberLit.hlsl"

cbuffer DecalBuffer : register(b2)
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

    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.ClipPos = ApplyMVP(input.Position);
    output.ClipPos.z -= 0.0001f;
    output.UV = input.UV;
    return output;
}

PSOutput mainPS(PSInput input) : SV_Target
{
    PSOutput output;
    
    float4 localPos = mul(float4(input.WorldPos, 1.0f), InvDecalWorld);
    clip(0.5f - abs(localPos.xyz));
    
    float2 decalUV;
    decalUV.xy = localPos.yz + 0.5f;
    decalUV.y = 1.0f - decalUV.y;
    
    float4 decalTex = DiffuseMap.Sample(SampleState, decalUV);
        
    float3 Albedo = decalTex.rgb * DecalColorTint.rgb;
    
    FUberMaterialInput MaterialInput;
    MaterialInput.Albedo = Albedo;
    MaterialInput.Emissive = float3(0.0f, 0.0f, 0.0f);
    MaterialInput.SpecularColor = float3(0.0f, 0.0f, 0.0f);
    MaterialInput.Shininess = 1.0f;

    FUberLightingInput LightingInput;
    LightingInput.WorldPos = input.WorldPos;
    LightingInput.WorldNormal = input.WorldNormal;
    LightingInput.ViewDir = normalize(CameraPosition - input.WorldPos);

    float3 LitColor = EvaluateUberLighting(MaterialInput, LightingInput).Color;

    output.Color = float4(LitColor, decalTex.a * DecalColorTint.a);
    
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
}
