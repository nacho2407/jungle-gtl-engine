/*
    Functions.hlsl는 셰이더 전역에서 재사용하는 공용 유틸리티 함수를 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

#ifndef FUNCTIONS_HLSL
#define FUNCTIONS_HLSL

#include "../Resources/ConstantBuffers.hlsl"
#include "../Common/VertexLayouts.hlsl"

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

float4 ApplyVP(float3 worldPos)
{
    return mul(mul(float4(worldPos, 1.0f), View), Projection);
}

float3 ApplyWireframe(float3 baseColor)
{
    return lerp(baseColor, WireframeRGB, bIsWireframe);
}

bool ShouldDiscardFontPixel(float sampledRed)
{
    return sampledRed < 0.1f;
}

PS_Input_UV FullscreenTriangleVS(uint vertexID)
{
    PS_Input_UV output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

#endif
