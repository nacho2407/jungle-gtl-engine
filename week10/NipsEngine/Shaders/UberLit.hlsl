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
    float2 padding2;
    
    float3 EmissiveColor;    // emissive glow color; non-zero means emissive
    float padding3;
};

#if HAS_DIFFUSE_MAP
Texture2D DiffuseMap  : register(t0);
#endif
#if HAS_NORMAL_MAP
Texture2D BumpMap : register(t1);
#endif
#if HAS_EMISSIVE_MAP
Texture2D AmbientMap  : register(t2);
#endif
#if HAS_SPECULAR_MAP
Texture2D SpecularMap : register(t3);
#endif

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
#elif HAS_NORMAL_MAP
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
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    
#if HAS_NORMAL_MAP
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3)WorldInvTrans)), input.Tangent.w);
#endif
    
#if LIGHTING_MODEL_GOURAUD
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
#endif
    
    return output;
}

#if HAS_NORMAL_MAP
float3 PerturbNormal(float3 worldNormal, float4 worldTangent, float2 uv)
{
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent.xyz - dot(worldTangent.xyz, N) * N);
    float3 B = cross(N, T) * worldTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    float3 tn = BumpMap.Sample(SampleState, uv).rgb * 2.0f - 1.0f;
    return normalize(mul(tn, TBN));
}
#endif

PSOutput mainPS(PSInput input) : SV_TARGET
{
    PSOutput output;
    
    float4 DiffuseTex = float4(1.f, 1.f, 1.f, 1.f);
    #if HAS_DIFFUSE_MAP
        DiffuseTex = DiffuseMap.Sample(SampleState, input.UV);
        clip(DiffuseTex.a - 0.001f);
    #endif
    
    float3 FinalColor = DiffuseColor * DiffuseTex.rgb;
    
    if (any(EmissiveColor > 0.f))
    {
        // Emissive surface: write the glow color and mark normal.a = 2
        output.Color = float4(EmissiveColor, 1.f) * DiffuseTex;
        output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 2.f);
        output.WorldPos = float4(input.WorldPos, 1.f);
        return output;
    }

    float3 N = normalize(input.WorldNormal);
#if HAS_NORMAL_MAP
    N = PerturbNormal(input.WorldNormal, input.WorldTangent, input.UV);
#endif
    
    float3 accumulatedLight = float3(1, 1, 1);
    
#if LIGHTING_MODEL_GOURAUD
    accumulatedLight = input.LitColor;
    
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint  numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData  = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];

    accumulatedLight = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    
    #if LIGHTING_MODEL_LAMBERT
        accumulatedLight += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N);
    #elif LIGHTING_MODEL_PHONG
        accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos, CameraPosition - input.WorldPos, Shininess);
    #endif    

    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
    #if LIGHTING_MODEL_LAMBERT
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos);
    #elif LIGHTING_MODEL_PHONG
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos, CameraPosition - input.WorldPos, Shininess)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos, CameraPosition - input.WorldPos, Shininess);
    #endif
    }
#endif
    
    output.Color = float4(FinalColor * accumulatedLight, 1.0f);
    output.Normal = float4(N * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    
    if (bIsWireframe > 0.5f)
    {
        output.Color = float4(WireframeRGB, 1.f);
    }
    
    return output;
}
