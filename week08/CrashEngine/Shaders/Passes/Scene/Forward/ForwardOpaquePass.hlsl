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
#include "../../../Render/Scene/Shared/OpaquePassTypes.hlsli"
#include "../../../Render/Scene/Material/MaterialSampling.hlsli"
#include "../../../Surface/SurfaceTypes.hlsli"
#include "../../../Render/Scene/Lighting/DirectLighting.hlsli"

#ifndef FORWARD_ENABLE_DECAL
#define FORWARD_ENABLE_DECAL 0
#endif

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
    Output.SceneColor = ComputeLambertLighting(BaseColor, Surface.WorldNormal);
    
    // Add Local Lights
    for (int j = 0; j < NumLocalLights; ++j)
    {
        float3 LocalTerm = LocalLightLambertTerm(g_LightBuffer[j], Surface.WorldNormal, Input.worldPos);
        Output.SceneColor.rgb += BaseColor.rgb * LocalTerm;
    }
    Output.SceneColor.rgb = saturate(Output.SceneColor.rgb);
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
        ViewDir);

    // Add Local Lights
    for (int j = 0; j < NumLocalLights; ++j)
    {
        FLocalBlinnPhongTerm LocalTerm = LocalLightBlinnPhongTerm(
            g_LightBuffer[j],
            Surface.WorldNormal,
            Input.worldPos,
            ViewDir,
            Surface.Roughness,
            Surface.Specular);
        Output.SceneColor.rgb += BaseColor.rgb * LocalTerm.Diffuse + LocalTerm.Specular;
    }
    Output.SceneColor.rgb = saturate(Output.SceneColor.rgb);
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
