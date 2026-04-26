/*
    ForwardOpaquePass.hlsl는 forward opaque pass의 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - t20~24: ShadowMap
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp, s3: Shadow
    - u#: Compute/후처리용 UAV
*/

/*
    ForwardOpaquePass.hlsl
    - Role: forward opaque geometry shading
    - Main bindings: b0 Frame, b1 PerObject, b2 StaticMeshMaterial, b4 GlobalLight,
      t0 BaseColor, t1 Normal, t2 Specular, t6 LocalLights
    - Available preprocessor defines:
      - LIGHTING_MODEL_GOURAUD
      - LIGHTING_MODEL_LAMBERT
      - LIGHTING_MODEL_BLINNPHONG
      - LIGHTING_MODEL_WORLDNORMAL
      - LIGHTING_MODEL_UNLIT
      - USE_NORMAL_MAP
      - FORWARD_ENABLE_LIGHTING
      - FORWARD_ENABLE_DECAL
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Resources/BindingSlots.hlsli"
#include "../../../Render/Scene/Shared/OpaquePassTypes.hlsli"
#include "../../../Render/Scene/Decal/DecalTypes.hlsli"
#include "../../../Render/Scene/Decal/DecalSampling.hlsli"
#include "../../../Render/Scene/Decal/DecalApply.hlsli"
#include "../../../Render/Scene/Material/MaterialSampling.hlsli"
#include "../../../Surface/SurfaceTypes.hlsli"
#include "../../../Render/Scene/Lighting/DirectLighting.hlsli"

#ifndef FORWARD_ENABLE_DECAL
#define FORWARD_ENABLE_DECAL 0
#endif

// Forward decals are intended to be folded into this opaque pass.
// The renderer currently binds the first visible decal as a shared forward input.

#ifndef FORWARD_ENABLE_LIGHTING
#define FORWARD_ENABLE_LIGHTING 1
#endif

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#define FORWARD_NORMAL_TEXTURE g_NormalMap
#else
#define FORWARD_NORMAL_TEXTURE g_txColor
#endif

Texture2D g_SpecularMap : register(t2);

#define MAX_FORWARD_DECAL_TEXTURES 8

StructuredBuffer<FForwardDecalData> g_ForwardDecalData : register(t12);
StructuredBuffer<uint> g_ForwardDecalIndices : register(t14);
Texture2D g_ForwardDecalTextures[MAX_FORWARD_DECAL_TEXTURES] : register(t32);

FForward_Opaque_VSOutput VS_ForwardOpaque(VS_Input_PNCT_T Input)
{
    FForward_Opaque_VSOutput Output;
    Output.position         = ApplyMVP(Input.position);
    Output.worldPos         = mul(float4(Input.position, 1.0f), Model).xyz;
    Output.worldNormal      = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w   = Input.tangent.w;
    Output.color            = Input.color;
    Output.texcoord         = Input.texcoord;
    Output.gouraud          = float4(ComputeGouraudLightingColor(Output.worldNormal, Output.worldPos), 1.0f);
    return Output;
}

float3 ResolveForwardOpaqueNormal(FForward_Opaque_VSOutput Input, Texture2D NormalMap)
{
    float3 N = normalize(Input.worldNormal);

#if defined(USE_NORMAL_MAP)
    if (StaticMeshHasNormalTexture())
    {
        float3 T = normalize(Input.worldTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * Input.worldTangent.w;
        float3x3 TBN = float3x3(T, B, N);
        float3 NormalSample = NormalMap.Sample(LinearWrapSampler, Input.texcoord).rgb;
        float3 TangentNormal = NormalSample * 2.0f - 1.0f;
        return normalize(mul(TangentNormal, TBN));
    }
#endif

    return N;
}

float4 ResolveForwardOpaqueMaterialParam(FForward_Opaque_VSOutput Input)
{
    float Shininess        = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;

    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= g_SpecularMap.Sample(LinearWrapSampler, Input.texcoord).r;
    }

    return float4(Shininess, SpecularStrength, 0.0f, 1.0f);
}

float4 SampleForwardDecalTexture(uint TextureIndex, float2 UV)
{
    switch (TextureIndex)
    {
    case 0: return g_ForwardDecalTextures[0].Sample(LinearWrapSampler, UV);
    case 1: return g_ForwardDecalTextures[1].Sample(LinearWrapSampler, UV);
    case 2: return g_ForwardDecalTextures[2].Sample(LinearWrapSampler, UV);
    case 3: return g_ForwardDecalTextures[3].Sample(LinearWrapSampler, UV);
    case 4: return g_ForwardDecalTextures[4].Sample(LinearWrapSampler, UV);
    case 5: return g_ForwardDecalTextures[5].Sample(LinearWrapSampler, UV);
    case 6: return g_ForwardDecalTextures[6].Sample(LinearWrapSampler, UV);
    case 7: return g_ForwardDecalTextures[7].Sample(LinearWrapSampler, UV);
    default: return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void ApplySingleForwardDecal(inout FSurfaceData Surface, float3 WorldPosition, FForwardDecalData DecalData)
{
#if FORWARD_ENABLE_DECAL
    float4 LocalPos = mul(float4(WorldPosition, 1.0f), DecalData.WorldToDecal);
    if (any(abs(LocalPos.xyz) > 0.5f))
    {
        return;
    }

    float2 DecalUV = ProjectDecalUV(LocalPos.xyz);
    float4 DecalSample = SampleForwardDecalTexture(DecalData.TextureIndex, DecalUV) * DecalData.Color;
    if (DecalSample.a <= 0.0f)
    {
        return;
    }

    float Alpha = DecalSample.a;
    float4 BaseColor = ApplyDecalBaseColor(float4(Surface.BaseColor, Surface.Opacity), DecalSample, Alpha);
    Surface.BaseColor = BaseColor.rgb;
    Surface.Opacity   = BaseColor.a;

// #if defined(LIGHTING_MODEL_LAMBERT) || defined(LIGHTING_MODEL_BLINNPHONG) || defined(LIGHTING_MODEL_WORLDNORMAL)
//     Surface.WorldNormal = ApplyDecalNormal(Surface.WorldNormal, DecalSample, Alpha);
// #endif

// #if defined(LIGHTING_MODEL_BLINNPHONG)
//     float4 MaterialParam = ApplyDecalMaterialParam(float4(Surface.Roughness, Surface.Specular, 0.0f, 1.0f), DecalSample, Alpha);
//     Surface.Roughness = MaterialParam.x;
//     Surface.Specular  = MaterialParam.y;
// #endif
#endif
}

void ApplyForwardDecal(inout FSurfaceData Surface, float3 WorldPosition)
{
#if FORWARD_ENABLE_DECAL
    [loop]
    for (uint DecalListIndex = 0; DecalListIndex < PrimitiveDecalCount; ++DecalListIndex)
    {
        uint DecalIndex = g_ForwardDecalIndices[PrimitiveDecalIndexOffset + DecalListIndex];
        ApplySingleForwardDecal(Surface, WorldPosition, g_ForwardDecalData[DecalIndex]);
    }
#endif
}

FSurfaceData BuildForwardSurfaceData(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = (FSurfaceData)0;
    float4 BaseColor = SampleStaticMeshBaseColor(g_txColor, Input.texcoord) * GetStaticMeshSectionColorOrWhite();
    float4 MaterialInfo = ResolveForwardOpaqueMaterialParam(Input);
    Surface.BaseColor        = BaseColor.rgb;
    Surface.Opacity          = BaseColor.a;
    Surface.WorldNormal      = ResolveForwardOpaqueNormal(Input, FORWARD_NORMAL_TEXTURE);
    Surface.Roughness        = MaterialInfo.x;
    Surface.Specular         = MaterialInfo.y;
    Surface.Metallic         = 0.0f;
    Surface.AmbientOcclusion = 1.0f;
    Surface.Gouraud          = Input.gouraud;
    ApplyForwardDecal(Surface, Input.worldPos);
    return Surface;
}

#if defined(LIGHTING_MODEL_GOURAUD)
FSceneColorOutput PS_Forward_Gouraud(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = BuildForwardSurfaceData(Input);
    FSceneColorOutput Output;
    float4 BaseColor = float4(Surface.BaseColor, Surface.Opacity);
    Output.SceneColor = ComputeGouraudLighting(BaseColor, Surface.Gouraud);
    return Output;
}
#endif

#if defined(LIGHTING_MODEL_UNLIT)
FSceneColorOutput PS_Forward_Unlit(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = BuildForwardSurfaceData(Input);
    FSceneColorOutput Output;
    Output.SceneColor = float4(Surface.BaseColor, Surface.Opacity);
    return Output;
}
#endif

#if defined(LIGHTING_MODEL_LAMBERT)
FSceneColorOutput PS_Forward_Lambert(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = BuildForwardSurfaceData(Input);
    FSceneColorOutput Output;
    float4 BaseColor = float4(Surface.BaseColor, Surface.Opacity);

#if FORWARD_ENABLE_LIGHTING
    Output.SceneColor = ComputeLambertLighting(BaseColor, Surface.WorldNormal, Input.worldPos);
#else
    Output.SceneColor = BaseColor;
#endif

    return Output;
}
#endif

#if defined(LIGHTING_MODEL_BLINNPHONG)
FSceneColorOutput PS_Forward_BlinnPhong(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = BuildForwardSurfaceData(Input);
    FSceneColorOutput Output;
    float4 BaseColor = float4(Surface.BaseColor, Surface.Opacity);

#if FORWARD_ENABLE_LIGHTING
    float3 ViewDir = normalize(CameraWorldPos - Input.worldPos);
    Output.SceneColor = ComputeBlinnPhongLighting(
        BaseColor,
        Surface.WorldNormal,
        float4(Surface.Roughness, Surface.Specular, 0.0f, 1.0f),
        Input.worldPos,
        ViewDir,
        Input.position);
#else
    Output.SceneColor = BaseColor;
#endif

    return Output;
}
#endif

#if defined(LIGHTING_MODEL_WORLDNORMAL)
FSceneColorOutput PS_Forward_WorldNormal(FForward_Opaque_VSOutput Input)
{
    FSurfaceData Surface = BuildForwardSurfaceData(Input);
    FSceneColorOutput Output;
    Output.SceneColor = float4(Surface.WorldNormal * 0.5f + 0.5f, Surface.Opacity);
    return Output;
}
#endif

#undef FORWARD_NORMAL_TEXTURE
