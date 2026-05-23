#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

// t0: 머티리얼 디퓨즈 텍스처
Texture2D MeshParticleTexture : register(t0);

// ──────────────────────────────────────────
// VS
//  slot 0 : 메시 정점 (FVertexPNCT)
//  slot 1 : 인스턴스 (FMeshParticleInstanceVertex — Transform + Color)
// ──────────────────────────────────────────
struct PS_Input_MeshParticle
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR;
};

PS_Input_MeshParticle VS(VS_Input_PNCT vert, VS_Input_MeshParticleInstance inst)
{
    // inst.transform : 파티클 1개의 World Transform (Scale * Rotation * Translation)
    float4 worldPos    = mul(float4(vert.position, 1.0f), inst.transform);
    float3 worldNormal = mul(float4(vert.normal,   0.0f), inst.transform).xyz;

    PS_Input_MeshParticle output;
    output.position = mul(worldPos, mul(View, Projection));
    output.normal   = normalize(worldNormal);
    output.texcoord = vert.texcoord;
    output.color    = vert.color * inst.color;  // 메시 버텍스 컬러 × 인스턴스 컬러
    return output;
}

// ──────────────────────────────────────────
// PS
// ──────────────────────────────────────────
float4 PS(PS_Input_MeshParticle input) : SV_TARGET
{
    float4 col = MeshParticleTexture.Sample(LinearClampSampler, input.texcoord);
    col *= input.color;
    clip(col.a - 0.01f);
    return col;
}
