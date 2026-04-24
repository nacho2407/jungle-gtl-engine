
/*
    DecalPass.hlsl는 장면 렌더링 패스의 셰이더입니다.

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
    - 이 파일에서 직접 선언한 슬롯: t0, t1, t2, t3, b2
*/

#include "../../Common/Surface/CommonTypes.hlsli"
#include "../../Common/Surface/SurfaceData.hlsli"
#include "../../Common/Material/DecalCommon.hlsli"

Texture2D g_DecalTex : register(t0);
Texture2D g_BaseColorTex : register(t1);
Texture2D g_Surface1Tex : register(t2);
Texture2D g_Surface2Tex : register(t3);

cbuffer DecalBuffer : register(b2)
{
    float4x4 DecalWorldToLocal;
    float4 DecalColor;
}

bool SampleDecalData(float2 UV, out float4 DecalSample, out float4 BaseColor, out float4 Surface1, out float4 Surface2)
{
    DecalSample = 0.0f;
    BaseColor = 0.0f;
    Surface1 = 0.0f;
    Surface2 = 0.0f;

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
    Surface1 = g_Surface1Tex.Sample(PointClampSampler, UV);
    Surface2 = g_Surface2Tex.Sample(PointClampSampler, UV);
    return true;
}

// 정점 입력을 화면 공간 출력으로 변환하는 버텍스 셰이더입니다.
PS_Input_UV VS_DecalFullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

PS_Input_UV VS(uint VertexID : SV_VertexID)
{
    // 정점 입력을 화면 공간 출력으로 변환하는 버텍스 셰이더입니다.
    return VS_DecalFullscreen(VertexID);
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
float4 PS_Decal_Unlit(PS_Input_UV Input) : SV_TARGET0
{
    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    return ApplyDecalBaseColor(BaseColor, DecalSample, DecalSample.a);
}

float4 PS(PS_Input_UV Input) : SV_TARGET0
{
    // 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
    return PS_Decal_Unlit(Input);
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
float4 PS_Decal_Gouraud(PS_Input_UV Input) : SV_TARGET0
{
    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
    {
        discard;
    }

    return ApplyDecalBaseColor(BaseColor, DecalSample, DecalSample.a);
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
FDecalOutput2 PS_Decal_Lambert(PS_Input_UV Input)
{
    FDecalOutput2 Output = (FDecalOutput2)0;

    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
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

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
FDecalOutput3 PS_Decal_BlinnPhong(PS_Input_UV Input)
{
    FDecalOutput3 Output = (FDecalOutput3)0;

    float4 DecalSample;
    float4 BaseColor;
    float4 Surface1;
    float4 Surface2;
    if (!SampleDecalData(Input.uv, DecalSample, BaseColor, Surface1, Surface2))
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

