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

struct FDecalInfo
{
    row_major matrix InvDecalWorld;
    float4 DecalColorTint;
    uint TextureIndex;
    float3 Padding;
};
StructuredBuffer<FDecalInfo> Decals : register(t8);
Texture2DArray DecalDiffuseTexture : register(t9);

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

#if LIGHT_HEATMAP
float3 GetHeatmapColor(float weight)
{
    float3 color;
    color.r = smoothstep(0.4f, 0.7f, weight);
    color.g = smoothstep(0.0f, 0.4f, weight) - smoothstep(0.7f, 1.0f, weight);
    color.b = 1.0f - smoothstep(0.0f, 0.4f, weight);
    return color;
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
    
    float4 FinalColor = float4(DiffuseColor * DiffuseTex.rgb, 1);
    
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
    
#if LIGHT_HEATMAP
    uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
    uint  numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint2 tileData  = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];

    float weight = saturate((float)tileData.y / 64.0f); // MAX_LIGHTS_PER_TILE 기준
    float3 heatmapColor = GetHeatmapColor(weight);
    
    // 타일 경계선 시각화 (선택 사항: 타일의 가장자리 1픽셀을 어둡게 처리)
    uint2 pixelInTile = uint2(input.ClipPos.xy) % TILE_SIZE;
    if (pixelInTile.x == 0 || pixelInTile.y == 0)
    {
        heatmapColor *= 0.5f; 
    }

    output.Color = float4(heatmapColor, 1.0f);
    output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    return output;
#endif
    
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
        accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, CameraPosition - input.WorldPos.xyz, Shininess);
    #endif

    for (uint i = 0; i < tileData.y; i++)
    {
        LightInfo light = Lights[CulledIndexBuffer[tileData.x + i]];
    #if LIGHTING_MODEL_LAMBERT
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz);
    #elif LIGHTING_MODEL_PHONG
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, CameraPosition - input.WorldPos.xyz, Shininess)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, CameraPosition - input.WorldPos.xyz, Shininess);
    #endif
        //accumulatedLight = float4(1,1,1,1);
    }
#endif
    
    float4 DecalColor = float4(0, 0, 0, 0);
    for (uint i = 0; i < DecalCount; i++)
    {
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
    output.Color = float4(FinalColor.xyz * accumulatedLight, 1.0f);
    output.Normal = float4(N * 0.5f + 0.5f, 1.f);
    output.WorldPos = float4(input.WorldPos, 1.f);
    
    if (bIsWireframe > 0.5f)
    {
        output.Color = float4(WireframeRGB, 1.f);
    }
    
    return output;
}
