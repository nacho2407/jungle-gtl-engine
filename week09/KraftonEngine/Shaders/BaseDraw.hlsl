#include "CommonTypes.hlsli"
#include "SurfaceData.hlsli"
#include "LightingCommon.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#endif

float3 ResolveBaseDrawNormal(FBaseDrawVSOutput Input)
{
    float3 NormalWS = normalize(Input.worldNormal);

#if defined(USE_NORMAL_MAP)
    float3 SampledNormal = g_NormalMap.Sample(LinearWrapSampler, Input.texcoord).xyz * 2.0f - 1.0f;
    NormalWS = normalize(lerp(NormalWS, SampledNormal, 0.5f));
#endif

    return NormalWS;
}

float4 ResolveBaseDrawColor(FBaseDrawVSOutput Input)
{
    float4 BaseSample = SampleBaseTexture(g_txColor, Input.texcoord);
    return BaseSample * Input.color * GetSectionColorOrWhite();
}

FBaseDrawVSOutput VS_BaseDraw(VS_Input_PNCT Input)
{
    FBaseDrawVSOutput Output;
    Output.position = ApplyMVP(Input.position);
    Output.worldNormal = normalize(mul(Input.normal, (float3x3)Model));
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
