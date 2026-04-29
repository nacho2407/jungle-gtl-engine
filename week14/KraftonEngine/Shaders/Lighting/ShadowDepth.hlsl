// =============================================================================
// ShadowDepth.hlsl — Shadow Depth Pass VS / PS
// =============================================================================
// DrawShadowCasters 전용 셰이더.
// b1(PerObjectBuffer)에 Model, b2(PerShader0)에 LightViewProj가 바인딩된 상태.
//
// Hard/PCF: PS=null (depth-only) — C++ 측에서 PSSetShader(nullptr)
// VSM:      PS가 depth moment(z, z^2)를 RTV에 기록
// =============================================================================

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"

// b2: Light ViewProj — Shadow depth pass 전용
cbuffer ShadowLightBuffer : register(b2)
{
    float4x4 LightViewProj;
};

// =============================================================================
// Vertex Shader — position-only transform (Model * LightViewProj)
// =============================================================================
// InputLayout은 VS_Input_PNCTT(StaticMesh)와 호환.
// Normal/Color/TexCoord/Tangent는 무시하고 Position만 사용.
PS_Input_Shadow VS(VS_Input_PNCTT input)
{
    PS_Input_Shadow output;

    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    float4 clipPos  = mul(worldPos, LightViewProj);

    output.position = clipPos;
    output.depth    = clipPos.z / clipPos.w; // Reversed-Z: near=1, far=0

    return output;
}

// =============================================================================
// Pixel Shader — EVSM moment 출력 (Hard/PCF 모드에서는 바인딩하지 않음)
// =============================================================================
// RTV format: R32G32_FLOAT — (moment1, moment2) = (exp(c*d), exp(2c*d))
// EVSM: 지수 워프로 깊이 분포를 분리하여 light bleeding 대폭 감소
float2 PS(PS_Input_Shadow input) : SV_TARGET
{
    float d = input.depth;
    float e = exp(EVSM_EXPONENT * d);

    float dx = ddx(e);
    float dy = ddy(e);

    // moment2에 partial derivative bias 추가 (shadow acne 완화)
    float moment2 = e * e + 0.25f * (dx * dx + dy * dy);

    return float2(e, moment2);
}
