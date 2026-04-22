#include "../../Common/Types/CommonTypes.hlsli"
#include "../../Common/Types/SurfaceData.hlsli"
#include "../../Common/Types/LightingCommon.hlsli"

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

FOpaqueVSOutput VS_Opaque(VS_Input_PNCT_T Input)
{
    FOpaqueVSOutput Output;
    Output.position = ApplyMVP(Input.position);
    
    // ?�드 ?��? �??�젠??변??(?�규???�함)
    float3 VSNormal = normalize(mul(Input.normal, (float3x3) NormalMatrix));
    Output.worldNormal = VSNormal;
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3) NormalMatrix));
    Output.worldTangent.w = Input.tangent.w;
    Output.color = Input.color;
    Output.texcoord = Input.texcoord;

    // Gouraud Shading???�점 ?�이??계산???�해 ?�드 ?��???계산
    // float4(pos, 1.0f)�?w=1??명시?�야 Model ?�렬???�동 ?�분???�용??(??그럴 ??w=0 ?�며 ?�아�?
    float3 WorldPos = mul(float4(Input.position, 1.0f), Model).xyz;
    float3 GouraudLighting = ComputeGouraudLightingColor(VSNormal, WorldPos);
    Output.gouraud = float4(GouraudLighting, 1.0f);

    return Output;
}

float4 PS_Opaque_Unlit(FOpaqueVSOutput Input) : SV_TARGET0
{
    return EncodeBaseColor(ResolveOpaqueColor(Input));
}

FOpaqueOutput2 PS_Opaque_Gouraud(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    // ?�점?�서 계산???�이??값을 그�?�?G-Buffer(Surface1)??기록
    Output.Surface1 = Input.gouraud;
    return Output;
}

FOpaqueOutput2 PS_Opaque_Lambert(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    Output.Surface1 = EncodeNormal(ResolveOpaqueNormal(Input));
    return Output;
}

FOpaqueOutput3 PS_Opaque_BlinnPhong(FOpaqueVSOutput Input)
{
    FOpaqueOutput3 Output;
    Output.BaseColor = EncodeBaseColor(ResolveOpaqueColor(Input));
    Output.Surface1 = EncodeNormal(ResolveOpaqueNormal(Input));
    
    // SpecularStrength�?0.3?�로 ??��???�이?�이?��? ?�얗�??�버리???�상??방�?
    float Shininess = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;
    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= g_SpecularMap.Sample(LinearWrapSampler, Input.texcoord).r;
    }
    Output.Surface2 = EncodeMaterialParam(float4(Shininess, SpecularStrength, 0.0f, 1.0f));
    return Output;
}