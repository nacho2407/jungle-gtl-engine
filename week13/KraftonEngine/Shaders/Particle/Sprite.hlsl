#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D ParticleTexture : register(t0);

// b2: 카메라 Right/Up (빌보드 확장용 — FFrameContext에서 매 프레임 업데이트)
cbuffer ParticleFrameBuffer : register(b2)
{
    float3 CameraRight;
    float _pad0;
    float3 CameraUp;
    float _pad1;
}

struct PS_Input_Particle
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR;
};

PS_Input_Particle VS(VS_Input_ParticleQuad quad, VS_Input_ParticleInstance inst)
{
    float sinR = sin(inst.rotation);
    float cosR = cos(inst.rotation);

    float2 rotUV = float2(
        quad.cornerUV.x * cosR - quad.cornerUV.y * sinR,
        quad.cornerUV.x * sinR + quad.cornerUV.y * cosR
    );

    float3 worldPos = inst.position
                    + CameraRight * rotUV.x * inst.size
                    + CameraUp * rotUV.y * inst.size;

    PS_Input_Particle output;
    output.position = mul(float4(worldPos, 1.0f), mul(View, Projection));
    output.texcoord = quad.cornerUV + 0.5f;
    output.color = inst.color;
    return output;
}

float4 PS(PS_Input_Particle input) : SV_TARGET
{
    float4 col = ParticleTexture.Sample(LinearClampSampler, input.texcoord);
    col *= input.color;
    clip(col.a - 0.01f);
    return col;
}