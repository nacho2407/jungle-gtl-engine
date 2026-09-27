#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"
#include "common/Functions.hlsl"

Texture2D DepthTexture : register(t1);
Texture2D NormalTexture : register(t2);
SamplerState g_Sample : register(s0);

struct PS_Input_Fireball
{
    float4 position : SV_POSITION;
    float4 clipPos : TEXCOORD0;
};

PS_Input_Fireball VS(uint vid : SV_VertexID)
{
    PS_Input_Fireball output;
    
    float2 pos = float2((vid == 2) ? 3.0f : -1.0f, (vid == 1) ? 3.0f : -1.0f);
    output.position = float4(pos, 0.0f, 1.0f);
    output.clipPos = output.position;

    return output;
}

float4 PS(PS_Input_Fireball input) : SV_TARGET
{
    float2 ndc = input.clipPos.xy / input.clipPos.w;
    float2 uv = ndc * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    float depth = DepthTexture.Sample(g_Sample, uv).r;
    if (depth >= 0.9999f)
        discard;

    float4 clip = float4(ndc, depth, 1.0f);
    float4 viewPos = mul(clip, InvProjMatrix);
    viewPos /= viewPos.w;

    float4 worldPos4 = mul(viewPos, InvViewMatrix);
    float3 worldPos = worldPos4.xyz;

    float3 fireballPos = Model[3].xyz;
    float dist = length(worldPos - fireballPos);

    if (dist > Radius)
        discard;

    float3 normalSample = NormalTexture.Sample(g_Sample, uv).xyz;
    float3 normalWS = normalize(normalSample * 2.0f - 1.0f);

    float3 toCenter = normalize(fireballPos - worldPos);
    float normalFactor = saturate(dot(normalWS, toCenter));

    // 거리 정규화 (0: 중심, 1: 반지름 경계)
    float radiusSafe = max(Radius, 0.0001f);
    float dist01 = saturate(dist / radiusSafe);

    // 거리 그라데이션 (중심 강, 가장자리 약)
    float radial = 1.0f - dist01;
    float distanceWeight = pow(radial, max(RadiusFalloff, 0.01f));

    // 경계 부드럽게
    float edgeFade = 1.0f - smoothstep(Radius * 0.85f, Radius, dist);
    distanceWeight *= edgeFade;

    // 수직(dot=0) 최소 밝기 보장 + 거리 기반 조정
    // 중심부 최소값 높게, 가장자리 최소값 낮게
    float normalMin = lerp(0.05f, 0.25f, radial);

    // 노멀 조명
    float lighting = lerp(normalMin, 1.0f, normalFactor);

    float3 rgb = Color.rgb * Intensity * distanceWeight * lighting;
    float alpha = Color.a * Intensity * distanceWeight * lighting;

    return float4(rgb, alpha);
}