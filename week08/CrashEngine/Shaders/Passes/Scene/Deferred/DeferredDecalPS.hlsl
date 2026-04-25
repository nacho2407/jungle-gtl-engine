/*
    DeferredDecalPS.hlsl는 deferred decal pass의 셰이더입니다.

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
    - none
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Render/Scene/Shared/OpaquePassTypes.hlsli"
#include "../../../Surface/SurfacePacking.hlsli"
#include "../../../Render/Scene/Decal/DecalSampling.hlsli"
#include "../../../Render/Scene/Decal/DecalApply.hlsli"
#include "../../../Resources/SystemResources.hlsl"
#include "../../../Resources/SystemSamplers.hlsl"

Texture2D g_DecalTex : register(t0);
Texture2D g_BaseColorTex : register(t1);
Texture2D g_Surface1Tex : register(t2);
Texture2D g_Surface2Tex : register(t3);

cbuffer DecalParams : register(b2)
{
    float4x4 DecalWorldToLocal;
    float4 DecalColor;
}

bool SampleDeferredDecalData(float2 UV, out float4 DecalSample, out float4 BaseColor, out float4 Surface1, out float4 Surface2)
{
    DecalSample = 0.0f;
    BaseColor   = 0.0f;
    Surface1    = 0.0f;
    Surface2    = 0.0f;

    float Depth = SceneDepth.Sample(PointClampSampler, UV).r;
    if (Depth <= 0.0f)
    {
        return false;
    }

    float3 WorldPosition = ReconstructPositionFromDepth(UV, Depth, InvViewProj);
    float3 LocalPosition = mul(float4(WorldPosition, 1.0f), DecalWorldToLocal).xyz;
    if (!IsInsideDecalBounds(LocalPosition))
    {
        return false;
    }

    float2 DecalUV = ProjectDecalUV(LocalPosition);
    DecalSample = g_DecalTex.Sample(LinearWrapSampler, DecalUV) * DecalColor;
    if (DecalSample.a <= 0.001f)
    {
        return false;
    }

    BaseColor = g_BaseColorTex.Sample(PointClampSampler, UV);
    Surface1  = g_Surface1Tex.Sample(PointClampSampler, UV);
    Surface2  = g_Surface2Tex.Sample(PointClampSampler, UV);
    return true;
}

PS_Input_UV VS_DeferredDecalFullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

PS_Input_UV VS(uint VertexID : SV_VertexID)
{
    return VS_DeferredDecalFullscreen(VertexID);
}

float4 PS_Decal_Unlit(PS_Input_UV Input) : SV_TARGET0
{
    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDeferredDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    return ApplyDecalBaseColor(BaseColor, DecalSample, DecalSample.a);
}

float4 PS(PS_Input_UV Input) : SV_TARGET0
{
    return PS_Decal_Unlit(Input);
}

float4 PS_Decal_Gouraud(PS_Input_UV Input) : SV_TARGET0
{
    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDeferredDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    return ApplyDecalBaseColor(BaseColor, DecalSample, DecalSample.a);
}

FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1 PS_Decal_Lambert(PS_Input_UV Input)
{
    FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1 Output = (FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1)0;

    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDeferredDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    float Alpha = DecalSample.a;
    Output.ModifiedBaseColor = ApplyDecalBaseColor(BaseColor, DecalSample, Alpha);

    float4 EncodedNormal = EncodeNormal(ApplyDecalNormal(DecodeNormal(Surface1), DecalSample, Alpha));
    EncodedNormal.a = Alpha;
    Output.ModifiedSurface1 = EncodedNormal;
    return Output;
}

FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1_ModifiedSurface2 PS_Decal_BlinnPhong(PS_Input_UV Input)
{
    FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1_ModifiedSurface2 Output = (FDeferred_Decal_Output_ModifiedBaseColor_ModifiedSurface1_ModifiedSurface2)0;

    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDeferredDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    float Alpha = DecalSample.a;
    Output.ModifiedBaseColor = ApplyDecalBaseColor(BaseColor, DecalSample, Alpha);

    float4 EncodedNormal = EncodeNormal(ApplyDecalNormal(DecodeNormal(Surface1), DecalSample, Alpha));
    EncodedNormal.a = Alpha;
    Output.ModifiedSurface1 = EncodedNormal;
    Output.ModifiedSurface2 = ApplyDecalMaterialParam(Surface2, DecalSample, Alpha);
    return Output;
}
