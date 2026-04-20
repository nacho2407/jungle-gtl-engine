#include "../Common/Utils/Functions.hlsl"
#include "../Common/Types/SurfaceData.hlsli"

Texture2D NormalTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float4 encodedNormal = NormalTexture.Sample(LinearClampSampler, input.uv);
    float3 normal = DecodeNormal(encodedNormal);
    return float4(normal * 0.5f + 0.5f, 1.0f);
}
