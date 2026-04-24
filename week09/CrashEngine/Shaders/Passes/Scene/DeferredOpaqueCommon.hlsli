#ifndef DEFERRED_OPAQUE_COMMON_HLSLI
#define DEFERRED_OPAQUE_COMMON_HLSLI

float3 ResolveDeferredOpaqueNormal(FOpaqueVSOutput Input, Texture2D NormalMap)
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

float4 ResolveDeferredOpaqueColor(FOpaqueVSOutput Input, Texture2D BaseColorTex)
{
    float4 BaseSample = SampleStaticMeshBaseColor(BaseColorTex, Input.texcoord);
    return BaseSample * GetStaticMeshSectionColorOrWhite();
}

FOpaqueVSOutput VS_DeferredOpaque(VS_Input_PNCT_T Input)
{
    FOpaqueVSOutput Output;
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

#endif
