#ifndef NORMAL_MAPPING_H
#define NORMAL_MAPPING_H

float3 PerturbNormal(Texture2D NormalMap, SamplerState NormalSampler, float3 WorldNormal, float4 WorldTangent, float2 UV)
{
    float3 N = normalize(WorldNormal);
    float3 T = normalize(WorldTangent.xyz - dot(WorldTangent.xyz, N) * N);
    float3 B = cross(N, T) * WorldTangent.w;
    float3x3 TBN = float3x3(T, B, N);
    float3 TangentNormal = NormalMap.Sample(NormalSampler, UV).rgb * 2.0f - 1.0f;
    return normalize(mul(TangentNormal, TBN));
}

#endif
