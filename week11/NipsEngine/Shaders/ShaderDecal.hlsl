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

LightResult EvaluateLightByType(FLightInfo LightData, float3 normal, float3 worldPos, float3 camPos, float Shininess)
{
    switch (LightData.Type)
    {
        case 0:
#if LIGHTING_MODEL_GOURAUD
            return EvaluateSpotlightGouraud(LightData.Color, LightData.Intensity, normal, LightData.Position, worldPos,
                                            LightData.Radius, LightData.Falloff, LightData.Direction, LightData.InnerAngle, LightData.OuterAngle, camPos - worldPos, Shininess);
#elif LIGHTING_MODEL_LAMBERT
            return EvaluateSpotlightLambert(LightData.Color, LightData.Intensity, normal, LightData.Position, worldPos,
                                            LightData.Radius, LightData.Falloff, LightData.Direction, LightData.InnerAngle, LightData.OuterAngle);
#elif LIGHTING_MODEL_PHONG
            return EvaluateSpotlightBlinnPhong(LightData.Color, LightData.Intensity, normal, LightData.Position, worldPos,
                                               LightData.Radius, LightData.Falloff, LightData.Direction, LightData.InnerAngle, LightData.OuterAngle, camPos - worldPos, Shininess);
#endif
            return (LightResult) 0;
        case 1:
#if LIGHTING_MODEL_GOURAUD
            return EvaluatePointGouraud(LightData.Color, LightData.Intensity, LightData.Position, worldPos, normal, LightData.Radius, LightData.Falloff, camPos - worldPos, Shininess);
#elif LIGHTING_MODEL_LAMBERT
            return EvaluatePointLambert(LightData.Color, LightData.Intensity, normal, LightData.Position, worldPos, LightData.Radius, LightData.Falloff);
#elif LIGHTING_MODEL_PHONG
            return EvaluatePointBlinnPhong(LightData.Color, LightData.Intensity, LightData.Position, worldPos, normal, LightData.Radius, LightData.Falloff, camPos - worldPos, Shininess);
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

    output.WorldNormal = normalize(mul(input.Normal, (float3x3)WorldInvTrans));
    output.ClipPos = ApplyMVP(input.Position);
    output.ClipPos.z -= 0.0001f;
    output.UV = input.UV;
    
#if LIGHTING_MODEL_GOURAUD

#elif LIGHTING_MODEL_LAMBERT
#elif LIGHTING_MODEL_PHONG
    return output;
#else
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
#endif
    
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
    
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];

#if LIGHTING_MODEL_GOURAUD
    output.Color = decalTex * DecalColorTint;
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
#elif LIGHTING_MODEL_LAMBERT
    float3 accumulated_light = float3(0, 0, 0);
    for (uint j = 0; j < tileData.y; j++)
    {
        LightResult result = EvaluateLightByType(Lights[CulledIndexBuffer[tileData.x + j]], input.WorldNormal, input.WorldPos, CameraPosition, 10.0f);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }
    
    LightResult AResult = EvaluateAmbientLight(AmbientLight.Color, AmbientLight.Intensity);
    accumulated_light += AResult.Diffuse + AResult.Ambient;
    
    float3 DColor = DirectionalLight.Color;
    float3 DDirection = DirectionalLight.Direction;
    float DIntensity = DirectionalLight.Intensity;
    LightResult DResult = EvaluateDirectionalLambert(DColor, DIntensity, DDirection, input.WorldNormal);
    accumulated_light += DResult.Diffuse + DResult.Ambient;
    
    output.Color = float4(decalTex.rgb * DecalColorTint.rgb * accumulated_light, decalTex.a * DecalColorTint.a);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
#elif LIGHTING_MODEL_PHONG
    float3 accumulated_light = float3(0, 0, 0);
    for (uint j = 0; j < tileData.y; j++)
    {
        LightResult result = EvaluateLightByType(Lights[CulledIndexBuffer[tileData.x + j]], input.WorldNormal, input.WorldPos, CameraPosition, 10.0f);
        accumulated_light += result.Diffuse + result.Specular + result.Ambient;
    }
    
    LightResult AResult = EvaluateAmbientLight(AmbientLight.Color, AmbientLight.Intensity);
    accumulated_light += AResult.Diffuse + AResult.Specular + AResult.Ambient;
    
    float3 DColor = DirectionalLight.Color;
    float3 DDirection = DirectionalLight.Direction;
    float DIntensity = DirectionalLight.Intensity;
    LightResult DResult = EvaluateDirectionalBlinnPhong(DColor, DIntensity, DDirection, input.WorldNormal, CameraPosition - input.WorldPos, 10.0f);
    accumulated_light += DResult.Diffuse + DResult.Specular + DResult.Ambient;
    
    output.Color = float4(decalTex.rgb * DecalColorTint.rgb * accumulated_light, decalTex.a * DecalColorTint.a);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
#else
    output.Color = decalTex * DecalColorTint;
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    
    return output;
#endif
}
