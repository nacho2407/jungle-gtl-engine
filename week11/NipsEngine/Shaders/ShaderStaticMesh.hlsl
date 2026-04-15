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
    float3 DiffuseTex = DiffuseMap.Sample(SampleState, input.UV).xyz;
    
    float3 FinalColor;
    
    if (!(bool)bHasDiffuseMap)
    {
        DiffuseTex = float3(1.f, 1.f, 1.f);
        FinalColor = DiffuseColor;
    }
    else
    {
        FinalColor = DiffuseTex;
    }
    //
    //float3 SpecularTex = SpecularMap.Sample(SampleState, input.UV).xyz;
    //
    //if (!(bool)bHasSpecularMap)
    //{
    //    SpecularTex = float3(1.f, 1.f, 1.f);
    //}
    //
    //float3 fixedLightDir = {1.f, -1.f, 1.f}; 
    //fixedLightDir = normalize(fixedLightDir);
    //
    //float3 N = normalize(input.WorldNormal);
    //
    // 퐁 셰이딩
    //float3 RelfectLightDir = 2.0f * dot(N, fixedLightDir) * N - fixedLightDir; 
    //RelfectLightDir = normalize(RelfectLightDir);
   
    //float3 ViewDir = CameraWorldPos - input.WorldPos;
    //ViewDir = normalize(ViewDir);
    
    // 블린 퐁 셰이딩 -> 반사 벡터 대신에 어정쩡한 벡터를 사용한다.
    //float3 HalfVector = normalize(fixedLightDir + ViewDir);
    //
    //float3 FinalAmbient = AmbientColor * DiffuseTex * 0.4f;
    //float3 FinalDiffuse = DiffuseTex * DiffuseColor * max(0, dot(N, fixedLightDir));
    //float3 FinalSpecular = SpecularTex * SpecularColor * pow(saturate(dot(HalfVector, ViewDir)), max(Shininess, 32.f));
    //
    //float3 FinalColor = FinalAmbient + FinalDiffuse + FinalSpecular;
    
    if (any(EmissiveColor > 0.f))
    {
        // Emissive surface: write the glow color and mark normal.a = 2
        float3 TexColor = bHasDiffuseMap ? DiffuseMap.Sample(SampleState, input.UV).xyz : float3(1.0f, 1.0f, 1.0f);
        output.Color    = float4(EmissiveColor * TexColor, 1.f);
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
