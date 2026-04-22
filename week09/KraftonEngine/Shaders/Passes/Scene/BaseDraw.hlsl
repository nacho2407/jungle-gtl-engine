#include "../../Common/Types/CommonTypes.hlsli"
#include "../../Common/Types/SurfaceData.hlsli"
#include "../../Common/Types/LightingCommon.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#endif

float3 ResolveBaseDrawNormal(FBaseDrawVSOutput Input)
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

float4 ResolveBaseDrawColor(FBaseDrawVSOutput Input)
{
    float4 BaseSample = SampleStaticMeshBaseColor(g_txColor, Input.texcoord);
    return BaseSample * GetStaticMeshSectionColorOrWhite();
}

FBaseDrawVSOutput VS_BaseDraw(VS_Input_PNCT_T Input)
{
    FBaseDrawVSOutput Output;
    Output.position = ApplyMVP(Input.position);
    
    // 월드 노멀 및 탄젠트 변환 (정규화 포함)
    float3 VSNormal = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldNormal = VSNormal;
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w = Input.tangent.w;
    Output.color = Input.color;
    Output.texcoord = Input.texcoord;

    // Gouraud Shading용 정점 라이팅 계산을 위해 월드 포지션 계산
    // float4(pos, 1.0f)로 w=1을 명시해야 Model 행렬의 이동 성분이 적용됨 (안 그럴 시 w=0 되며 날아감)
    float3 WorldPos = mul(float4(Input.position, 1.0f), Model).xyz;
    float3 GouraudLighting = ComputeGouraudLightingColor(VSNormal, WorldPos);
    Output.gouraud = float4(GouraudLighting, 1.0f);

    return Output;
}

float4 PS_BaseDraw_Unlit(FBaseDrawVSOutput Input) : SV_TARGET0
{
    return EncodeBaseColor(ResolveBaseDrawColor(Input));
}

FBaseDrawOutput2 PS_BaseDraw_Gouraud(FBaseDrawVSOutput Input)
{
    FBaseDrawOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveBaseDrawColor(Input));
    // 정점에서 계산된 라이팅 값을 그대로 G-Buffer(Surface1)에 기록
    Output.Surface1 = Input.gouraud;
    return Output;
}

FBaseDrawOutput2 PS_BaseDraw_Lambert(FBaseDrawVSOutput Input)
{
    FBaseDrawOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveBaseDrawColor(Input));
    Output.Surface1 = EncodeNormal(ResolveBaseDrawNormal(Input));
    return Output;
}

FBaseDrawOutput3 PS_BaseDraw_BlinnPhong(FBaseDrawVSOutput Input)
{
    FBaseDrawOutput3 Output;
    Output.BaseColor = EncodeBaseColor(ResolveBaseDrawColor(Input));
    Output.Surface1 = EncodeNormal(ResolveBaseDrawNormal(Input));
    
    // SpecularStrength를 0.3으로 낮춰서 하이라이트가 하얗게 타버리는 현상을 방지
    float Shininess = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;
    Output.Surface2 = EncodeMaterialParam(float4(Shininess, SpecularStrength, 0.0f, 1.0f));
    return Output;
}
