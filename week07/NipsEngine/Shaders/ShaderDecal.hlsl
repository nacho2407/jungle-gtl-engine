#include "Common.hlsl"
#include "Lighting.hlsl"

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
    float4 Tangent : TANGENT;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
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
    float4 Color : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 WorldPos : SV_TARGET2;
};


PSInput mainVS(VSInput input)
{
    PSInput output;
    
    output.WorldPos = mul(float4(input.Position, 1.0f), Model).xyz;
    output.ClipPos = ApplyMVP(input.Position);
    output.ClipPos.z -= 0.0001f;
    output.UV = input.UV;
    
#if LIGHTING_MODEL_GOURAUD
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    
    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, 32.0f);
    
    for (uint i = 0; i < LightCount; i++) {
        LightInfo light = Lights[i];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, 32.0f) :
            CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, CameraPosition - output.WorldPos, 32.0f);
    }
    
    output.LitColor = accumulatedLight;
    return output;
    
#elif LIGHTING_MODEL_LAMBERT
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3) WorldInvTrans)), input.Tangent.w);
    return output;
    
#elif LIGHTING_MODEL_PHONG
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    output.PixelNormal = output.WorldNormal;
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3) WorldInvTrans)), input.Tangent.w);
    return output;
#else
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    return output;
    
#endif
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
    
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];

#if LIGHTING_MODEL_GOURAUD
    output.Color = float4(input.LitColor, 1.0f) * decalTex * DecalColorTint;
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;

#elif LIGHTING_MODEL_LAMBERT
    float3 N_Lambert = normalize(input.WorldNormal);
    
    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N_Lambert);
    
    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), N_Lambert, input.WorldPos)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), N_Lambert, input.WorldPos);
    }
    
    output.Color = decalTex * DecalColorTint * float4(accumulatedLight, 1.0f);
    output.Normal = float4(N_Lambert * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;
    
#elif LIGHTING_MODEL_PHONG
    float3 N_Phong = normalize(input.PixelNormal);
    
    float3 accumulatedLight = float3(0, 0, 0);
    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, 32.0f);
    
    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, 32.0f)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N_Phong, input.WorldPos, CameraPosition - input.WorldPos, 32.0f);
    }
    
    output.Color = float4(accumulatedLight, 1.0f) * decalTex * DecalColorTint;
    output.Normal = float4(N_Phong * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;
    
#else
    output.Color = decalTex * DecalColorTint;
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;
#endif
}
