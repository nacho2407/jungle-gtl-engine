/*
    DeferredOpaquePass.hlsl는 deferred opaque pass의 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

/*
    Available preprocessor defines:
    - LIGHTING_MODEL_GOURAUD
    - LIGHTING_MODEL_LAMBERT
    - LIGHTING_MODEL_BLINNPHONG
    - LIGHTING_MODEL_UNLIT
    - OUTPUT_GOURAUD_L
    - OUTPUT_NORMAL
    - OUTPUT_MATERIAL_PARAM
    - USE_NORMAL_MAP
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Render/Scene/Shared/OpaquePassTypes.hlsli"
#include "../../../Render/Scene/Material/MaterialSampling.hlsli"
#include "../../../Surface/SurfaceTypes.hlsli"
#include "../../../Surface/GBufferPacking.hlsli"
#include "../../../Render/Scene/Lighting/DirectLighting.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#define DEFERRED_NORMAL_TEXTURE g_NormalMap
#else
#define DEFERRED_NORMAL_TEXTURE g_txColor
#endif

// TODO: specular도 매크로 분기
Texture2D g_SpecularMap : register(t2);

float3 ResolveDeferredOpaqueNormal(FDeferred_Opaque_VSOutput Input, Texture2D NormalMap)
{
    float3 N = normalize(Input.worldNormal);

#if defined(USE_NORMAL_MAP)
    if (StaticMeshHasNormalTexture())
    {
        float3 T = normalize(Input.worldTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * Input.worldTangent.w;
        float3x3 TBN = float3x3(T, B, N);

        float3 NormalSample  = NormalMap.Sample(LinearWrapSampler, Input.texcoord).rgb;
        float3 TangentNormal = NormalSample * 2.0f - 1.0f;
        return normalize(mul(TangentNormal, TBN));
    }
#endif

    return N;
}

float4 ResolveDeferredOpaqueMaterialParam(FDeferred_Opaque_VSOutput Input, Texture2D SpecularMap)
{
    float Shininess        = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;

    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= SpecularMap.Sample(LinearWrapSampler, Input.texcoord).r;
    }

    return float4(Shininess, SpecularStrength, 0.0f, 1.0f);
}

FSurfaceData BuildDeferredSurfaceData(FDeferred_Opaque_VSOutput Input)
{
    FSurfaceData Surface = (FSurfaceData)0;
    float4 BaseColor = SampleStaticMeshBaseColor(g_txColor, Input.texcoord) * GetStaticMeshSectionColorOrWhite();
    float4 MaterialInfo = ResolveDeferredOpaqueMaterialParam(Input, g_SpecularMap);
    Surface.BaseColor        = BaseColor.rgb;
    Surface.Opacity          = BaseColor.a;
    Surface.WorldNormal      = ResolveDeferredOpaqueNormal(Input, DEFERRED_NORMAL_TEXTURE);
    Surface.Roughness        = MaterialInfo.x;
    Surface.Specular         = MaterialInfo.y;
    Surface.Metallic         = 0.0f;
    Surface.AmbientOcclusion = 1.0f;
    Surface.Gouraud          = Input.gouraud;
    return Surface;
}

FDeferred_Opaque_VSOutput VS_DeferredOpaque(VS_Input_PNCT_T Input)
{
    FDeferred_Opaque_VSOutput Output;
    Output.position         = ApplyMVP(Input.position);
    Output.worldNormal      = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w   = Input.tangent.w;
    Output.color            = Input.color;
    Output.texcoord         = Input.texcoord;

    float3 WorldPos        = mul(float4(Input.position, 1.0f), Model).xyz;
    float3 GouraudLighting = ComputeGouraudLightingColor(Output.worldNormal, WorldPos);
    Output.gouraud         = float4(GouraudLighting, 1.0f);
    return Output;
}

float4 PS_Opaque_Unlit(FDeferred_Opaque_VSOutput Input) : SV_TARGET0
{
    FSurfaceData Surface = BuildDeferredSurfaceData(Input);
    return EncodeBaseColor(float4(Surface.BaseColor, Surface.Opacity));
}

FGBufferOutput2 PS_Opaque_Gouraud(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_Gouraud(BuildDeferredSurfaceData(Input));
}

FGBufferOutput2 PS_Opaque_Lambert(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_Lambert(BuildDeferredSurfaceData(Input));
}

FGBufferOutput3 PS_Opaque_BlinnPhong(FDeferred_Opaque_VSOutput Input)
{
    return EncodeGBuffer_BlinnPhong(BuildDeferredSurfaceData(Input));
}

#undef DEFERRED_NORMAL_TEXTURE
