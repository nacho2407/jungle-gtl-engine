#include "Common.hlsl"

cbuffer StaticMeshBuffer : register(b6)
{
    float3 AmbientColor;    // Ka
    float3 DiffuseColor;    // Kd
    float3 SpecularColor;   // Ks
    float  Shininess;       // Ns
    // Camera
    float3 CameraWorldPos;
    // ScrollUV
    float2 ScrollUV;
    float  Padding6_1;
    
    uint   bHasDiffuseMap;   
    uint   bHasSpecularMap;  
    float  Padding6_2;       
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

    output.WorldPos    = mul(float4(input.Position, 1.0f), Model).xyz;
    
    // 비균일 스케일을 위한 역행렬 이후 전치 
    // 역행렬은 비용이 많이 들어서 상수 버퍼로 가져오는 게 나을 거 같네요...
    float3x3 normalMatrix = transpose(Inverse3x3((float3x3) Model));
    output.WorldNormal = normalize(mul(input.Normal, normalMatrix));
    output.UV          = input.UV + ScrollUV;
    output.ClipPos     = ApplyMVP(input.Position);
    return output;
}


float4 mainPS(PSInput input) : SV_TARGET
{
    float3 DiffuseTex = DiffuseMap.Sample(SampleState, input.UV).xyz;
    
    if (!(bool)bHasDiffuseMap)
    {
        DiffuseTex = float3(1.f, 1.f, 1.f);
    }
    
    float3 SpecularTex = SpecularMap.Sample(SampleState, input.UV).xyz;
    
    if (!(bool)bHasSpecularMap)
    {
        SpecularTex = float3(1.f, 1.f, 1.f);
    }
    
    float3 fixedLightDir = {1.f, -1.f, 1.f}; 
    fixedLightDir = normalize(fixedLightDir);
    
    float3 N = normalize(input.WorldNormal);
    
    // 퐁 셰이딩
    //float3 RelfectLightDir = 2.0f * dot(N, fixedLightDir) * N - fixedLightDir; 
    //RelfectLightDir = normalize(RelfectLightDir);
   
    float3 ViewDir = CameraWorldPos - input.WorldPos;
    ViewDir = normalize(ViewDir);
    
    // 블린 퐁 셰이딩 -> 반사 벡터 대신에 어정쩡한 벡터를 사용한다.
    float3 HalfVector = normalize(fixedLightDir + ViewDir);
    
    float3 FinalAmbient = AmbientColor * DiffuseTex * 0.2f;
    float3 FinalDiffuse = DiffuseTex * DiffuseColor * max(0, dot(N, fixedLightDir));
    float3 FinalSpecular = SpecularTex * SpecularColor * pow(saturate(dot(HalfVector, ViewDir)), max(Shininess, 32.f));

    float3 FinalColor = FinalAmbient + FinalDiffuse + FinalSpecular;
    return float4(FinalColor, 1.f);
}
