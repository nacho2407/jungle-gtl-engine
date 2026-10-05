#include "../Common/Common.hlsli"

Texture2D DiffuseMap : register(t0);
SamplerState SampleState : register(s0);

cbuffer ParticleMaterialBuffer : register(b2)
{
    float Opacity;
    float3 ParticleMaterialPadding;
};

struct VSInput
{
    float3 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float3 source : POSITION1;
    float3 target : POSITION2;
    float halfWidth : TEXCOORD1;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput VS(VSInput input)
{
    PSInput output;

    const float3 source = input.source;
    const float3 target = input.target;
    const float3 center = (source + target) * 0.5f;
    const float3 beamVector = target - source;
    const float beamLength = max(length(beamVector), 0.001f);
    const float3 beamDir = beamVector / beamLength;
    const float3 viewDir = normalize(CameraPosition - center);

    float3 side = cross(viewDir, beamDir);
    if (dot(side, side) < 1e-5f)
    {
        side = cross(float3(0.0f, 0.0f, 1.0f), beamDir);
    }
    if (dot(side, side) < 1e-5f)
    {
        side = cross(float3(0.0f, 1.0f, 0.0f), beamDir);
    }
    side = normalize(side) * input.halfWidth;

    const float along = input.position.x * 0.5f + 0.5f;
    const float across = input.position.y;
    const float3 worldPosition = lerp(source, target, along) + side * across;

    output.position = mul(mul(float4(worldPosition, 1.0f), View), Projection);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 diffuse = DiffuseMap.Sample(SampleState, input.texCoord);
    float4 color = input.color * diffuse;
    color.a *= Opacity;

    if (color.a <= 0.001f)
    {
        discard;
    }

    if (bIsWireframe > 0.5f)
    {
        return float4(WireframeRGB, color.a);
    }

    return color;
}
