#include "CommonTypes.hlsli"
#include "SurfaceData.hlsli"
#include "LightingCommon.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#endif

float3 ResolveBaseDrawNormal(FBaseDrawVSOutput Input)
{
    float3 N = normalize(Input.worldNormal);

#if defined(USE_NORMAL_MAP)
    float3 T = normalize(Input.worldTangent.xyz);
    
    // 그람-슈미트 정규직교화
    T = normalize(T - dot(T, N) * N);
    
    // Bitangent 생성 (Handedness w 적용)
    float3 B = cross(N, T) * Input.worldTangent.w;
    
    float3x3 TBN = float3x3(T, B, N);

    float3 normalSample = g_NormalMap.Sample(LinearWrapSampler, Input.texcoord).rgb;
    
    // 노멀 맵 데이터가 있는 경우에만 TBN 변환 적용
    if (length(normalSample) > 0.0001f)
    {
        float3 tangentNormal = normalSample * 2.0f - 1.0f;
        return normalize(mul(tangentNormal, TBN));
    }
#endif

    return N;
}

float4 ResolveBaseDrawColor(FBaseDrawVSOutput Input)
{
    float4 BaseSample = SampleBaseTexture(g_txColor, Input.texcoord);
    return BaseSample * Input.color * GetSectionColorOrWhite();
}

FBaseDrawVSOutput VS_BaseDraw(VS_Input_PNCT_T Input)
{
    FBaseDrawVSOutput Output;
    Output.position = ApplyMVP(Input.position);
    
    // VS에서 정규화: 보간 왜곡을 방지하기 위해 1차 정규화 수행
    Output.worldNormal = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w = Input.tangent.w;

    Output.color = Input.color;
    Output.texcoord = Input.texcoord;

    float GouraudFactor = ComputeGouraudLightingFactor(Output.worldNormal);
    Output.gouraud = float4(GouraudFactor.xxx, 1.0f);

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
    Output.Surface1 = EncodeSurface1(Input.gouraud);
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
    Output.Surface2 = EncodeMaterialParam(float4(32.0f, 1.0f, 0.0f, 1.0f));
    return Output;
}
