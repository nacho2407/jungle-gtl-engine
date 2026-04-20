#include "Common.hlsl"
#include "Lighting.hlsl"

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
    uint bHasBumpMap;
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
    float4 Tangent  : TANGENT;
};

struct PSInput
{
    float4 ClipPos      : SV_POSITION;
    float3 WorldPos     : TEXCOORD0;
    float3 WorldNormal  : TEXCOORD1;
    float2 UV           : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor     : TEXCOORD3;
#elif LIGHTING_MODEL_LAMBERT
    float4 WorldTangent : TEXCOORD3;
#elif LIGHTING_MODEL_PHONG
    float3 PixelNormal  : TEXCOORD4;
    float4 WorldTangent : TEXCOORD5;
#endif
};

struct PSOutput
{
    float4 Color    : SV_TARGET0;
    float4 Normal   : SV_TARGET1;
    float4 WorldPos : SV_TARGET2;
};

PSInput mainVS(VSInput input)
{
    PSInput output;

    output.WorldPos = mul(float4(input.Position, 1.0f), Model).xyz;
    output.ClipPos = ApplyMVP(input.Position);
    output.UV = input.UV + ScrollUV;
        
#if LIGHTING_MODEL_GOURAUD
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    
    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, Shininess);
    
    for (uint i = 0; i < LightCount; i++) {
        LightInfo light = Lights[i];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, Shininess)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, Shininess);
    }
    
    output.LitColor = accumulatedLight;
    
    return output;
    
#elif LIGHTING_MODEL_LAMBERT
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3)WorldInvTrans)), input.Tangent.w);
    return output;
    
#elif LIGHTING_MODEL_PHONG
    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.PixelNormal = output.WorldNormal;
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3)WorldInvTrans)), input.Tangent.w);
    return output;
    
#else 
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    return output;
    
#endif
}

float3 PerturbNormal(float3 worldNormal, float4 worldTangent, float2 uv)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent.xyz - dot(worldTangent.xyz, N) * N);
    float3 B = cross(N, T) * worldTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    float3 tn = BumpMap.Sample(SampleState, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(tn, TBN));
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

    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint  numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData  = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];
    
#if LIGHTING_MODEL_GOURAUD
    output.Color = float4(input.LitColor * FinalColor, 1.0);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
    
#elif LIGHTING_MODEL_LAMBERT
    float3 N_Lambert = (bHasBumpMap)
        ? PerturbNormal(input.WorldNormal, input.WorldTangent, input.UV)
        : normalize(input.WorldNormal);

    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N_Lambert);
    
    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), input.WorldNormal, input.WorldPos)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), input.WorldNormal, input.WorldPos);
    }

    output.Color = float4(FinalColor * accumulatedLight, 1.f);
    output.Normal = float4(N_Lambert * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
    
#elif LIGHTING_MODEL_PHONG
    if (bHasBumpMap)
    {
        float3 rawSample = BumpMap.Sample(SampleState, input.UV).rgb;
        output.Color    = float4(rawSample, 1.f);
        output.Normal   = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
        output.WorldPos = float4(input.WorldPos, 1.f);
    }
    
    float3 N_Phong = (bHasBumpMap)
        ? PerturbNormal(input.PixelNormal, input.WorldTangent, input.UV)
        : normalize(input.PixelNormal);

    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, Shininess);
    
    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, Shininess)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, Shininess);
    }

    output.Color = float4(FinalColor * accumulatedLight, 1.f);
    output.Normal = float4(N_Phong * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
    
#else
    output.Color = float4(FinalColor, 1.0f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
    
#endif
}
