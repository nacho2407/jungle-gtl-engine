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

struct FLightInfo
{
    float3 Color;
    float Intensity;

    uint  Type;
    float Radius;
    float InnerAngle;
    float OuterAngle;

    float3 Direction;
    float  Falloff;

    float3 Position;
    float  Padding1;
};
StructuredBuffer<FLightInfo> Lights : register(t4);

struct VSInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORD;
    float4 Color    : COLOR;
    float3 Normal   : NORMAL;
};

struct PSInput
{
    float4 ClipPos      : SV_POSITION;
    float3 WorldPos     : TEXCOORD0;
    float3 WorldNormal  : TEXCOORD1;
    float2 UV           : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor    : TEXCOORD3;
#elif LIGHTING_MODEL_PHONG
    float3 PixelNormal : TEXCOORD4;
#endif
};

struct PSOutput
{
    float4 Color    : SV_TARGET0;
    float4 Normal   : SV_TARGET1;
    float4 WorldPos : SV_TARGET2;
};

LightResult EvaluateLightByType(FLightInfo LightData, float3 normal, float3 worldPos, float3 camPos, float Shininess)
{
    switch (LightData.Type)
    {
        case 0:
            return EvaluateAmbientLight(LightData.Color, LightData.Intensity);
        case 1:
#if LIGHTING_MODEL_GOURAUD
            return EvaluateDirectionalGouraud(LightData.Color, LightData.Intensity, LightData.Direction, normal, camPos - worldPos, Shininess);
#elif LIGHTING_MODEL_LAMBERT
            return EvaluateDirectionalLambert(LightData.Color, LightData.Intensity, LightData.Direction, normal);
#elif LIGHTING_MODEL_PHONG
            return EvaluateDirectionalBlinnPhong(LightData.Color, LightData.Intensity, LightData.Direction, normal, camPos - worldPos, Shininess);
#endif
            return (LightResult) 0;
        case 2:
#if LIGHTING_MODEL_GOURAUD
            return EvaluatePointGouraud(LightData.Color, LightData.Intensity, LightData.WorldPos, worldPos, normal, LightData.Radius, LightData.RadiusFalloff, camPos - worldPos, Shininess);
#elif LIGHTING_MODEL_LAMBERT
            return EvaluatePointLambert(LightData.Color, LightData.Intensity, normal, LightData.WorldPos, worldPos, LightData.Radius, LightData.RadiusFalloff);
#elif LIGHTING_MODEL_PHONG
            return EvaluatePointBlinnPhong(LightData.Color, LightData.Intensity, LightData.WorldPos, worldPos, normal, LightData.Radius, LightData.RadiusFalloff, camPos - worldPos, Shininess);
#endif
            return (LightResult) 0;
        case 3:
#if LIGHTING_MODEL_GOURAUD
        return EvaluateSpotlightGouraud(LightData.Color, LightData.Intensity, normal, LightData.WorldPos, worldPos,
                                        LightData.Radius, LightData.RadiusFalloff, LightData.Direction, LightData.InnerAngle, LightData.OuterAngle, camPos - worldPos, Shininess);
#elif LIGHTING_MODEL_LAMBERT
        return EvaluateSpotlightLambert(LightData.Color, LightData.Intensity, normal, LightData.WorldPos, worldPos,
                                        LightData.Radius, LightData.RadiusFalloff, LightData.Direction, LightData.InnerAngle, LightData.OuterAngle);
#elif LIGHTING_MODEL_PHONG
#endif
            return (LightResult) 0;
        default:
            return (LightResult) 0;
    }
}


PSInput mainVS(VSInput input)
{
    PSInput output;

    output.WorldPos = mul(float4(input.Position, 1.0f), Model).xyz;
    output.ClipPos = ApplyMVP(input.Position);
    output.UV = input.UV + ScrollUV;
    
#if LIGHTING_MODEL_GOURAUD
    float3 accumulated_light = float3(0, 0, 0);
    uint LightCount, stride;
    Lights.GetDimensions(LightCount, stride);
    for (uint i = 0; i < LightCount; i++) {
        LightResult result = EvaluateLightByType(Lights[i], output.WorldNormal, output.WorldPos, CameraPosition, Shininess);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }
    
    output.LitColor = accumulated_light;
    return output;
#elif LIGHTING_MODEL_LAMBERT
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    return output;
#elif LIGHTING_MODEL_PHONG
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.PixelNormal = output.WorldNormal;
    return output;
#else 
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
#endif
   
    return output;
}

PSOutput mainPS(PSInput input) : SV_TARGET
{
    PSOutput output;
    
    float4 DiffuseTex = float4(1.f, 1.f, 1.f, 1.f);
    if ((bool) bHasDiffuseMap)
    {
        DiffuseTex = DiffuseMap.Sample(SampleState, input.UV);
        clip(DiffuseTex.a - 0.001f);
    }
    
    float3 FinalColor = DiffuseColor * DiffuseTex.rgb;
    
    if (any(EmissiveColor > 0.f))
    {
        // Emissive surface: write the glow color and mark normal.a = 2
        output.Color = float4(EmissiveColor, 1.f) * DiffuseTex;
        output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 2.f);
        output.WorldPos = float4(input.WorldPos, 1.f);
        return output;
    }

    if (bIsWireframe > 0.5f)
    {
        FinalColor = WireframeRGB;
    }
    
    uint LightCount, stride;
    Lights.GetDimensions(LightCount, stride);
    
#if LIGHTING_MODEL_GOURAUD
    output.Color = float4(input.LitColor * FinalColor, 1.0);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
#elif LIGHTING_MODEL_LAMBERT
    float3 accumulated_light = float3(0, 0, 0);
    for (uint i = 0; i < LightCount; i++)
    {
        LightResult result = EvaluateLightByType(Lights[i], input.WorldNormal, input.WorldPos, CameraPosition, Shininess);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }

    output.Color = float4(FinalColor * accumulated_light, 1.f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    
    return output;
#elif LIGHTING_MODEL_PHONG
    float3 accumulated_light = float3(0, 0, 0);
    for (uint i = 0; i < LightCount; i++)
    {
        LightResult result = EvaluateLightByType(Lights[i], input.PixelNormal, input.WorldPos, CameraPosition, Shininess);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }

    output.Color = float4(FinalColor * accumulated_light, 1.f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    
    return output;
#else
    float3 accumulated_light = float3(0, 0, 0);
    for (uint i = 0; i < LightCount; i++)
    {
        LightResult result = EvaluateLightByType(Lights[i], input.WorldNormal, input.WorldPos, CameraPosition, Shininess);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }

    output.Color = float4(FinalColor * accumulated_light, 1.f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
#endif
}
