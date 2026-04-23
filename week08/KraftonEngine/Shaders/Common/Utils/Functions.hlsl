// Shader include: Common/Utils/Functions.hlsl
// Role: shared shader code or editor/material entry.
// Slots: declared locally or in included common resources.

#ifndef FUNCTIONS_HLSL
#define FUNCTIONS_HLSL

#include "../Resources/ConstantBuffers.hlsl"
#include "../Geometry/VertexLayouts.hlsl"

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

#endif // FUNCTIONS_HLSL

