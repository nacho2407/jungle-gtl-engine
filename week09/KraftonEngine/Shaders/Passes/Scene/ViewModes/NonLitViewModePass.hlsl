// Shader: NonLitViewModePass
// Role: fullscreen visualization for non-lit view modes.
// Defines: NON_LIT_VIEW_SCENE_DEPTH or NON_LIT_VIEW_WORLD_NORMAL.
// Entries: VS, PS. Slots: b2 SceneDepthCB, t0 NormalTexture, t10 SceneDepth, s0 LinearClamp.

#include "../../../Common/Utils/Functions.hlsl"
#include "../../../Common/Surface/SurfaceData.hlsli"
#include "../../../Common/Resources/SystemResources.hlsl"

#if defined(NON_LIT_VIEW_WORLD_NORMAL)
Texture2D NormalTexture : register(t0);
SamplerState LinearClampSampler : register(s0);
#endif

#if defined(NON_LIT_VIEW_SCENE_DEPTH)
cbuffer SceneDepthCB : register(b2)
{
    float Exponent;
    float NearClip;
    float FarClip;
    float Range;
    uint Mode;
    float3 _Padding;
}
#endif

PS_Input_UV VS(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS(PS_Input_UV Input) : SV_TARGET
{
#if defined(NON_LIT_VIEW_SCENE_DEPTH)
    int2 Coord = int2(Input.position.xy);
    float Depth = SceneDepth.Load(int3(Coord, 0)).r;

    if (Depth <= 0.0000001f)
    {
        return float4(0, 0, 0, 1);
    }

    float LinearZ = (NearClip * FarClip) / (Depth * (FarClip - NearClip) + NearClip);
    float Value = 0.0f;

    if (Mode == 1)
    {
        Value = frac((LinearZ - NearClip) / max(0.1f, Range));
    }
    else
    {
        Value = saturate((LinearZ - NearClip) / max(0.1f, Range));
    }

    Value = pow(Value, Exponent);
    return float4(Value.xxx, 1.0f);
#elif defined(NON_LIT_VIEW_WORLD_NORMAL)
    float4 EncodedNormal = NormalTexture.Sample(LinearClampSampler, Input.uv);
    float3 Normal = DecodeNormal(EncodedNormal);
    return float4(Normal * 0.5f + 0.5f, 1.0f);
#else
    return float4(0, 0, 0, 1);
#endif
}

