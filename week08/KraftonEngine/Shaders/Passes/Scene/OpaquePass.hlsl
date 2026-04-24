
/*
    OpaquePass.hlsl는 장면 렌더링 패스의 셰이더입니다.

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
    - 이 파일에서 직접 선언한 슬롯: t0, t1, t2
*/

#include "../../Common/Surface/CommonTypes.hlsli"
#include "../../Common/Surface/SurfaceData.hlsli"
#include "../../Common/Lighting/LightingCommon.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#endif

Texture2D g_SpecularMap : register(t2);

float3 ResolveOpaqueNormal(FOpaqueVSOutput Input)
{
    float3 N = normalize(Input.worldNormal);

#if defined(USE_NORMAL_MAP)
    if (StaticMeshHasNormalTexture())
    {
        float3 T = normalize(Input.worldTangent.xyz);
        T = normalize(T - dot(T, N) * N);
        float3 B = cross(N, T) * Input.worldTangent.w;
        float3x3 TBN = float3x3(T, B, N);

        float3 normalSample = g_NormalMap.Sample(LinearWrapSampler, Input.texcoord).rgb;
        float3 tangentNormal = normalSample * 2.0f - 1.0f;
        return normalize(mul(tangentNormal, TBN));
    }
#endif

    return N;
}

float4 ResolveOpaqueColor(FOpaqueVSOutput Input)
{
    float4 BaseSample = SampleStaticMeshBaseColor(g_txColor, Input.texcoord);
    return BaseSample * GetStaticMeshSectionColorOrWhite();
}

// 정점 입력을 화면 공간 출력으로 변환하는 버텍스 셰이더입니다.
FOpaqueVSOutput VS_Opaque(VS_Input_PNCT_T Input)
{
    FOpaqueVSOutput Output;
    Output.position = ApplyMVP(Input.position);
    
    float3 VSNormal = normalize(mul(Input.normal, (float3x3) NormalMatrix));
    Output.worldNormal = VSNormal;
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3) NormalMatrix));
    Output.worldTangent.w = Input.tangent.w;
    Output.color = Input.color;
    Output.texcoord = Input.texcoord;

    float3 WorldPos = mul(float4(Input.position, 1.0f), Model).xyz;
    float3 GouraudLighting = ComputeGouraudLightingColor(VSNormal, WorldPos);
    Output.gouraud = float4(GouraudLighting, 1.0f);

    return Output;
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
float4 PS_Opaque_Unlit(FOpaqueVSOutput Input) : SV_TARGET0
{
    return EncodeBaseColor(ResolveOpaqueColor(Input));
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
FOpaqueOutput2 PS_Opaque_Gouraud(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    Output.Surface1 = Input.gouraud;
    return Output;
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
FOpaqueOutput2 PS_Opaque_Lambert(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    Output.Surface1 = EncodeNormal(ResolveOpaqueNormal(Input));
    return Output;
}

// 래스터화된 픽셀의 최종 색상 또는 표면 데이터를 계산합니다.
FOpaqueOutput3 PS_Opaque_BlinnPhong(FOpaqueVSOutput Input)
{
    FOpaqueOutput3 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    Output.Surface1 = EncodeNormal(ResolveOpaqueNormal(Input));
    
    float Shininess = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;
    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= g_SpecularMap.Sample(LinearWrapSampler, Input.texcoord).r;
    }
    Output.Surface2 = EncodeMaterialParam(float4(Shininess, SpecularStrength, 0.0f, 1.0f));
    return Output;
}

