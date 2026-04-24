/*
    DeferredDecalPass.hlsl는 deferred 경로의 decal 장면 패스 셰이더입니다.
*/

#include "../../Common/Surface/CommonTypes.hlsli"
#include "../../Common/Surface/SurfaceData.hlsli"
#include "../../Common/Material/DecalCommon.hlsli"

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

FDecalOutput2 PS_Decal_Lambert(PS_Input_UV Input)
{
    FDecalOutput2 Output = (FDecalOutput2)0;

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

FDecalOutput3 PS_Decal_BlinnPhong(PS_Input_UV Input)
{
    FDecalOutput3 Output = (FDecalOutput3)0;

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
