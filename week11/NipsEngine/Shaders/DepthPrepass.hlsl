#include "Common.hlsl"

struct VSInput
{
    float3 Position : POSITION;
};

float4 DepthPrepassVS(VSInput input) : SV_POSITION
{
    return ApplyMVP(input.Position);
}
