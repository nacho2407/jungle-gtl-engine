#include "../Common/Common.hlsli"
#include "../Common/Lighting.hlsli"
#include "../Common/ShadowFunction.hlsli"

cbuffer StaticMeshBuffer : register(b2)
{
    float3 AmbientColor; // Ka
    float padding0;
    
    float3 DiffuseColor; // Kd
    float padding1;
    
    float3 SpecularColor; // Ks
    float Shininess; // Ns    
    
    float2 ScrollUV;
    float2 padding2;
    
    float3 EmissiveColor; // emissive glow color; non-zero means emissive
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
Texture2D EmissiveMap  : register(t2);
#endif
#if HAS_SPECULAR_MAP
Texture2D SpecularMap : register(t3);
#endif

Texture2D ShadowMap : register(t10);
TextureCubeArray<float> PointShadowCube : register(t12);

SamplerState SampleState : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

Texture2D VSMMap : register(t11); // ?곷떒 ?좎뼵遺??異붽?

struct VSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
};

struct SkeletalVSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};

struct PSInput
{
    float4 ClipPos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 UV : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float3 LitColor     : TEXCOORD3;
#elif HAS_NORMAL_MAP
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
    output.UV = input.UV + ScrollUV;
    output.WorldNormal = normalize(mul(input.Normal, (float3x3) WorldInvTrans));
    
#if HAS_NORMAL_MAP && !LIGHTING_MODEL_GOURAUD
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, (float3x3)WorldInvTrans)), input.Tangent.w);
#endif
    
#if LIGHTING_MODEL_GOURAUD
    float3 accumulatedLight = float3(0, 0, 0);
    float3 VertexSpecular = SpecularColor;
    float3 V = CameraPosition - output.WorldPos;
    if (IsOrthographic > 0.5f)
    {
        V = -float3(View[0].xyz);
    }

    accumulatedLight += CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, V, Shininess, VertexSpecular);
    
    for (uint i = 0; i < LightCount; i++) {
        LightInfo light = Lights[i];
        accumulatedLight += light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, V, Shininess, VertexSpecular)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), output.WorldNormal, output.WorldPos, V, Shininess, VertexSpecular);
    }
    
    output.LitColor = accumulatedLight;
#endif
    
    return output;
}

PSInput SkeletalMeshVS(SkeletalVSInput input)
{
    VSInput passThrough;
    passThrough.Position = input.Position;
    passThrough.Color = input.Color;
    passThrough.Normal = input.Normal;
    passThrough.UV = input.UV;
    passThrough.Tangent = input.Tangent;
    return mainVS(passThrough);
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

float GetCascadeSplitFarValue(uint CascadeIndex)
{
    return (CascadeIndex == 0) ? CascadeSplitFar.x :
           (CascadeIndex == 1) ? CascadeSplitFar.y :
           (CascadeIndex == 2) ? CascadeSplitFar.z :
                                 CascadeSplitFar.w;
}

float GetCameraDepthForCSM(float3 WorldPos)
{
    float4 ViewPos = mul(float4(WorldPos, 1.0f), View);
    return ViewPos.x;
}

uint SelectDirectionalCascade(float CameraDepth)
{
    for (uint i = 0; i < DirectionalCascadeCount; ++i)
    {
        if (CameraDepth <= GetCascadeSplitFarValue(i))
        {
            return i;
        }
    }

    return MAX_DIRECTIONAL_CASCADE_COUNT;
}

float ComputeBias(float LightSpaceZ, float ConstantBias, float SlopeScaleBias)
{
    // 媛??湲곗슱湲곌? ??寃껋쑝濡?SlopBias 媛以묒튂瑜?寃곗젙
    float dz_dx = ddx(LightSpaceZ);
    float dz_dy = ddy(LightSpaceZ);
    float slope = max(abs(dz_dx), abs(dz_dy));
    float slopeBias = slope * SlopeScaleBias;
    return ConstantBias + slopeBias;
}

float3 ComputeShadowCoordCascade(float4 worldPos, int CascadeIndex)
{
    FAtlasShadowData shadowData = AtlasShadowDatas[DirectionalShadowStartIndex + CascadeIndex];
    float4 shadowCoord = mul(worldPos, shadowData.ShadowViewProj);

    if (abs(shadowCoord.w) < 1e-5f)
    {
        return 1.0f;
    }

    return shadowCoord.xyz / shadowCoord.w;
}

#ifdef SHADOW_MAP_CSM
float CalculateShadow(float4 worldPos)
{
    if (DirectionalCascadeCount == 0 || DirectionalCascadeCount == 0)
    {
        return 1.0f;
    }

    float CameraDepth = GetCameraDepthForCSM(worldPos.xyz);
    uint CascadeIndex = SelectDirectionalCascade(CameraDepth);

    if (CascadeIndex >= DirectionalCascadeCount)
    {
        return 1.0f;
    }

    const float BlendAreaRatio = 0.1f;
    float CascadeFar  = GetCascadeSplitFarValue(CascadeIndex);
    float CascadeNear = (CascadeIndex == 0) ? 0.0f : GetCascadeSplitFarValue(CascadeIndex - 1);
    float BlendWidth  = (CascadeFar - CascadeNear) * BlendAreaRatio;

    float BlendFactor = saturate((CameraDepth - (CascadeFar - BlendWidth)) / BlendWidth);

    float3 projCoords = ComputeShadowCoordCascade(worldPos, CascadeIndex);
    
    float2 shadowUV = float2(
          projCoords.x * 0.5f + 0.5f,
          -projCoords.y * 0.5f + 0.5f
      );

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
          shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
          projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }
    
    FAtlasShadowData cascadeShadowData = AtlasShadowDatas[DirectionalShadowStartIndex + CascadeIndex];
    
    float totalBias = ComputeBias(projCoords.z, cascadeShadowData.ConstantBias, cascadeShadowData.SlopedBias);
#if SHADOW_MAP_VSM
    float ShadowFactor = ComputeShadowVSM(projCoords, cascadeShadowData.ScaleOffset, VSMMap, SampleState, 0.0001);
#else
    float ShadowFactor = ComputeShadowPCF(projCoords, cascadeShadowData.ScaleOffset, (int) cascadeShadowData.ShadowSoftness, ShadowSampler, ShadowMap, totalBias);
#endif   
    // 留덉?留??몃뜳???쒖쇅?섍퀬 釉붾젋??
    if (CascadeIndex < DirectionalCascadeCount - 1)
    {
        int NextCascade = CascadeIndex + 1;
        float3 NextCascadeProjCoords = ComputeShadowCoordCascade(worldPos, NextCascade);
        FAtlasShadowData nextCascadeShadowData = AtlasShadowDatas[DirectionalShadowStartIndex + NextCascade];
        float nextTotalBias = ComputeBias(NextCascadeProjCoords.z, nextCascadeShadowData.ConstantBias, nextCascadeShadowData.SlopedBias);

#if SHADOW_MAP_VSM        
        float NextCascadeShadowFactor = ComputeShadowVSM(NextCascadeProjCoords, nextCascadeShadowData.ScaleOffset , VSMMap, SampleState, 0.0001);        
#else 
        float NextCascadeShadowFactor = ComputeShadowPCF(NextCascadeProjCoords, nextCascadeShadowData.ScaleOffset, (int) nextCascadeShadowData.ShadowSoftness, ShadowSampler, ShadowMap, nextTotalBias);
#endif
        ShadowFactor = lerp(ShadowFactor, NextCascadeShadowFactor, BlendFactor);
    }
    
    return ShadowFactor;
}
#elif SHADOW_MAP_PSM
float CalculateShadow(float4 worldPos)
{
    float4 shadowCoord = float4(0.f, 0.f, 0.f, 1.f);
    float4 camClip = mul(worldPos, VirtualViewProj);

    if (abs(camClip.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 post = camClip.xyz / camClip.w;
    shadowCoord = mul(float4(post, 1.0f), ShadowViewProj);
    
    if (abs(shadowCoord.w) < 1e-5f)
    {
        return 1.0f;
    }

    float3 projCoords = shadowCoord.xyz / shadowCoord.w;
    float2 shadowUV = float2(
          projCoords.x * 0.5f + 0.5f,
          -projCoords.y * 0.5f + 0.5f
      );

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
          shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
          projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }
    
    FAtlasShadowData shadowData = AtlasShadowDatas[DirectionalShadowStartIndex];
    float totalBias = ComputeBias(projCoords.z, shadowData.ConstantBias, shadowData.SlopedBias);
   
#if SHADOW_MAP_VSM
    float shadowFactor = ComputeShadowVSM(projCoords, shadowData.ScaleOffset, VSMMap, SampleState, 0.0001);
#else
    float shadowFactor = ComputeShadowPCF(projCoords, shadowData.ScaleOffset, (int) shadowData.ShadowSoftness, ShadowSampler, ShadowMap, totalBias);
#endif
    return shadowFactor;
}
#else
    float CalculateShadow(float4 worldPos)
    {
        return 1.0f;
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
    float3 SpecularFactor = SpecularColor;
#if HAS_SPECULAR_MAP
    SpecularFactor *= SpecularMap.Sample(SampleState, input.UV).rgb;
#endif
    
    float3 Emissive = EmissiveColor;
#if HAS_EMISSIVE_MAP
    Emissive *= EmissiveMap.Sample(SampleState, input.UV).rgb;
#endif

    if (any(abs(Emissive) > 0.0001f))
    {
        // Emissive surface: keep base color visible, add glow color, and mark normal.a = 2.
        output.Color = float4(FinalColor.rgb + Emissive * DiffuseTex.rgb, 1.f);
        output.Normal = float4(input.WorldNormal * 0.5f + 0.5f, 2.f);
        output.WorldPos = float4(input.WorldPos, 1.f);
        return output;
    }

    float3 N = normalize(input.WorldNormal);
#if HAS_NORMAL_MAP && !LIGHTING_MODEL_GOURAUD
    N = PerturbNormal(input.WorldNormal, input.WorldTangent, input.UV);
#endif
    
    float3 accumulatedLight = float3(1, 1, 1);
    
#if LIGHT_HEATMAP
#if CULLING_MODEL_CLUSTERED
        uint2 tileCoord  = uint2(input.ClipPos.xy) / TILE_SIZE;
        uint  numTilesX  = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint  numTilesY  = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
        float z          = (IsOrthographic) ? NearZ + input.ClipPos.z * (FarZ - NearZ) : abs(Projection[3][2] / (input.ClipPos.z - Projection[0][2]));
    
        uint  sliceIndex = clamp(uint(log(z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
        uint2 clusterData = TileBuffer[(sliceIndex * numTilesY + tileCoord.y) * numTilesX + tileCoord.x];
        float weight = saturate(float(clusterData.y) / 64.0); // MAX_LIGHTS_PER_TILE 湲곗?
#elif CULLING_MODEL_TILED
        uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
        uint  numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint2 tileData  = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];
        float weight = saturate((float)tileData.y / 64.0f); // MAX_LIGHTS_PER_TILE 湲곗?
#endif
    float3 heatmapColor = GetHeatmapColor(weight);
    
    // ???寃쎄퀎???쒓컖??(?좏깮 ?ы빆: ??쇱쓽 媛?μ옄由?1?쎌????대몼寃?泥섎━)
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
#if CULLING_MODEL_CLUSTERED
        uint2 tileCoord  = uint2(input.ClipPos.xy) / TILE_SIZE;
        uint  numTilesX  = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint  numTilesY  = (uint(ViewportSize.y) + TILE_SIZE - 1) / TILE_SIZE;
        float z          = (IsOrthographic) ? NearZ + input.ClipPos.z * (FarZ - NearZ) : abs(Projection[3][2] / (input.ClipPos.z - Projection[0][2]));
        uint  sliceIndex = clamp(uint(log(z / NearZ) / log(FarZ / NearZ) * NUM_SLICE), 0, NUM_SLICE - 1);
        uint2 clusterData = TileBuffer[(sliceIndex * numTilesY + tileCoord.y) * numTilesX + tileCoord.x];
#elif CULLING_MODEL_TILED
        uint2 tileCoord = uint2(input.ClipPos.xy) / TILE_SIZE;
        uint  numTilesX = (uint(ViewportSize.x) + TILE_SIZE - 1) / TILE_SIZE;
        uint2 tileData  = TileBuffer[tileCoord.y * numTilesX + tileCoord.x];
#endif

    accumulatedLight = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    
    float shadowFactor = CalculateShadow(float4(input.WorldPos, 1.0f));
    
    float3 V = normalize(CameraPosition - input.WorldPos);
    if (IsOrthographic > 0.5f)
    {  
        V = normalize(-float3(View[0].xyz));
    }
    
#if LIGHTING_MODEL_LAMBERT
        accumulatedLight += CalcDirectionalLambert(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N) * shadowFactor;
#elif LIGHTING_MODEL_PHONG
        accumulatedLight += CalcDirectionalBlinnPhong(DirectionalLight, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, V, Shininess, SpecularFactor) * shadowFactor;
#endif

    uint LightsToIterate;
#if CULLING_MODEL_CLUSTERED
        LightsToIterate = clusterData.y;
#elif CULLING_MODEL_TILED
        LightsToIterate = tileData.y;
#else
        LightsToIterate = LightCount;
#endif 
    
    for (uint i = 0; i < LightsToIterate; i++)
    {
#if CULLING_MODEL_CLUSTERED
        uint lightIndex = CulledIndexBuffer[clusterData.x + i];
#elif CULLING_MODEL_TILED
        uint lightIndex = CulledIndexBuffer[tileData.x + i];
#else 
        uint lightIndex = i;
#endif
        LightInfo light = Lights[lightIndex];
        float lightShadowFactor = ComputeShadowAtlas(lightIndex, float4(input.WorldPos, 1.0f), ShadowSampler, ShadowMap, SampleState, PointShadowCube);
    
#if LIGHTING_MODEL_LAMBERT
        accumulatedLight += (light.Type == 0 ?
            CalcSpotlightLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz)
            : CalcPointLambert(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz)) * lightShadowFactor;
#elif LIGHTING_MODEL_PHONG
        accumulatedLight += (light.Type == 0 ?
            CalcSpotlightBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, V, Shininess, SpecularFactor)
            : CalcPointBlinnPhong(light, float3(1.0f, 1.0f, 1.0f), N, input.WorldPos.xyz, V, Shininess, SpecularFactor)) * lightShadowFactor;
#endif
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
    output.WorldPos = float4(input.WorldPos, 1.f);
    output.Normal = float4(N * 0.5f + 0.5f, 1.f);

    if (bIsWireframe > 0.5f)
    {
        output.Color = float4(WireframeRGB, 1.f);
    }

#ifdef CASCADE_VIS
    if (DirectionalCascadeCount > 0)
    {
        float CameraDepth = GetCameraDepthForCSM(input.WorldPos);
        uint CascadeIndex = SelectDirectionalCascade(CameraDepth);

        float3 cascadeColor;
        if      (CascadeIndex == 0) cascadeColor = float3(1.0f, 0.0f, 0.0f);
        else if (CascadeIndex == 1) cascadeColor = float3(0.0f, 1.0f, 0.0f);
        else if (CascadeIndex == 2) cascadeColor = float3(0.0f, 0.0f, 1.0f);
        else                        cascadeColor = float3(1.0f, 1.0f, 0.0f);

        if (CascadeIndex < MAX_DIRECTIONAL_CASCADE_COUNT)
            output.Color.rgb = lerp(output.Color.rgb, cascadeColor, 0.5f);
    }
#endif
 
   
      
    return output;
}
