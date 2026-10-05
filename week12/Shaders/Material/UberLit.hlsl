#include "../Common/Common.hlsli"
#include "../Common/SkeletalSkinning.hlsli"
#include "../Common/Lighting.hlsli"
#include "../Common/LightCulling.hlsli"
#include "../Common/NormalMapping.hlsli"
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

struct InstancedSurfaceVSInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
    float4 Tangent : TANGENT;
    row_major float4x4 InstanceTransform : TEXCOORD1;
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

void ApplyWireframeColor(inout float4 color)
{
    if (bIsWireframe > 0.5f)
    {
        color = float4(WireframeRGB, 1.0f);
    }
}

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
    FSkinningResult Skinned = ApplyLinearBlendSkinning(
        input.Position,
        input.Normal,
        input.Tangent.xyz,
        input.BoneIndices,
        input.BoneWeights);

    VSInput passThrough;
    passThrough.Position = Skinned.Position;
    passThrough.Color = input.Color;
    passThrough.Normal = Skinned.Normal;
    passThrough.UV = input.UV;
    passThrough.Tangent = float4(Skinned.Tangent, input.Tangent.w);

    return mainVS(passThrough);
}

PSInput InstancedSurfaceVS(InstancedSurfaceVSInput input)
{
    PSInput output;

    float4 world = mul(float4(input.Position, 1.0f), input.InstanceTransform);
    output.WorldPos = world.xyz;
    output.ClipPos = mul(mul(world, View), Projection);
    output.UV = input.UV + ScrollUV;

    float3x3 instanceBasis = (float3x3)input.InstanceTransform;
    output.WorldNormal = normalize(mul(input.Normal, instanceBasis));
#if HAS_NORMAL_MAP && !LIGHTING_MODEL_GOURAUD
    output.WorldTangent = float4(normalize(mul(input.Tangent.xyz, instanceBasis)), input.Tangent.w);
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

float4 mainPS(PSInput input) : SV_TARGET0
{
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
        float4 emissiveColor = float4(FinalColor.rgb + Emissive * DiffuseTex.rgb, 1.f);
        ApplyWireframeColor(emissiveColor);
        return emissiveColor;
    }

    float3 N = normalize(input.WorldNormal);
#if HAS_NORMAL_MAP && !LIGHTING_MODEL_GOURAUD
    N = PerturbNormal(BumpMap, SampleState, input.WorldNormal, input.WorldTangent, input.UV);
#endif
    
    float3 accumulatedLight = float3(1, 1, 1);
    
#if LIGHTING_MODEL_GOURAUD
    accumulatedLight = input.LitColor;
    
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    accumulatedLight = CalcAmbient(AmbientLight, float3(1.0f, 1.0f, 1.0f));
    
    float shadowFactor = CalculateDirectionalShadow(float4(input.WorldPos, 1.0f), ShadowSampler, ShadowMap, SampleState, VSMMap);
    
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

    FVisibleLightList VisibleLights = GetVisibleLightList(input.ClipPos);
    
    for (uint i = 0; i < VisibleLights.Count; i++)
    {
        uint lightIndex = GetVisibleLightIndex(VisibleLights, i);
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
    
    float4 outputColor = float4(FinalColor.xyz * accumulatedLight, 1.0f);
    ApplyWireframeColor(outputColor);

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
            outputColor.rgb = lerp(outputColor.rgb, cascadeColor, 0.5f);
    }
#endif
 
   
      
    return outputColor;
}
