#include "Common.hlsl"
#include "Lighting.hlsl"

SamplerState SampleState : register(s0);

struct FDecalInfo
{
    row_major matrix InvDecalWorld;
    float4 DecalColorTint;
    uint TextureIndex;
    float3 Padding;
};
StructuredBuffer<FDecalInfo> Decals : register(t7);
Texture2DArray DecalDiffuseTexture : register(t8);

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

    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];

    float4 FinalColor = float4(0, 0, 0, 0);

#if LIGHTING_MODEL_GOURAUD
    float4 DecalColor = float4(0, 0, 0, 0);
    for (uint i = 0; i < DecalCount; i++) {
        float4 DecalWorldPos = mul(float4(input.WorldPos, 1.0f), Decals[i].InvDecalWorld);
        if (any(abs(DecalWorldPos.xyz) > 0.5f))
            continue;

        float2 decalUV;
        decalUV.xy = DecalWorldPos.yz + 0.5f;
        decalUV.y = 1.0f - decalUV.y;

        float4 decalTex = DecalDiffuseTexture.SampleLevel(SampleState, float3(decalUV, Decals[i].TextureIndex), 0);
        DecalColor = (decalTex.a > 0.001) ? decalTex * Decals[i].DecalColorTint : DecalColor;
    }
    FinalColor = (DecalColor.a > 0.001) ? DecalColor : FinalColor;

    output.Color = float4(input.LitColor * FinalColor.rgb, FinalColor.a);
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

    float4 DecalColor = float4(0, 0, 0, 0);
    for (uint i = 0; i < DecalCount; i++) {
        float4 DecalWorldPos = mul(float4(input.WorldPos, 1.0f), Decals[i].InvDecalWorld);
        if (any(abs(DecalWorldPos.xyz) > 0.5f))
            continue;

        float2 decalUV;
        decalUV.xy = DecalWorldPos.yz + 0.5f;
        decalUV.y = 1.0f - decalUV.y;

        float4 decalTex = DecalDiffuseTexture.SampleLevel(SampleState, float3(decalUV, Decals[i].TextureIndex), 0);
        DecalColor = (decalTex.a > 0.001) ? decalTex * Decals[i].DecalColorTint : DecalColor;
    }
    FinalColor = (DecalColor.a > 0.001) ? DecalColor : FinalColor;

    output.Color = FinalColor * float4(accumulatedLight, 1.0f);
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

    float4 DecalColor = float4(0, 0, 0, 0);
    for (uint i = 0; i < DecalCount; i++) {
        float4 DecalWorldPos = mul(float4(input.WorldPos, 1.0f), Decals[i].InvDecalWorld);

        float2 decalUV;
        decalUV.xy = DecalWorldPos.yz + 0.5f;
        decalUV.y = 1.0f - decalUV.y;

        float4 decalTex = DecalDiffuseTexture.SampleLevel(SampleState, float3(decalUV, Decals[i].TextureIndex), 0);
        DecalColor = (decalTex.a > 0.001) ? decalTex * Decals[i].DecalColorTint : DecalColor;
    }
    FinalColor = (DecalColor.a > 0.001) ? DecalColor : FinalColor;
    output.Color = float4(accumulatedLight, 1.0f) * FinalColor;
    output.Normal = float4(N_Phong * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;

#else
    float4 DecalColor = float4(0, 0, 0, 0);
    for (uint i = 0; i < DecalCount; i++) {
        float4 DecalWorldPos = mul(float4(input.WorldPos, 1.0f), Decals[i].InvDecalWorld);
        if (any(abs(DecalWorldPos.xyz) > 0.5f))
            continue;

        float2 decalUV;
        decalUV.xy = DecalWorldPos.yz + 0.5f;
        decalUV.y = 1.0f - decalUV.y;

        float4 decalTex = DecalDiffuseTexture.SampleLevel(SampleState, float3(decalUV, Decals[i].TextureIndex), 0);
        DecalColor = (decalTex.a > 0.001) ? decalTex * Decals[i].DecalColorTint : DecalColor;
    }
    FinalColor = (DecalColor.a > 0.001) ? DecalColor : FinalColor;

    output.Color = FinalColor;
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.0f);
    return output;
#endif
}
