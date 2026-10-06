#include "Common/ConstantBuffers.hlsli"
#include "Common/SystemSamplers.hlsli"

Texture2D BeamTrailTexture : register(t0);

struct VS_Input_BeamTrail
{
    float3 position : POSITION;
    float relativeTime : RELATIVE_TIME;
    float3 oldPosition : OLD_POSITION;
    float particleId : PARTICLE_ID;
    float2 size : SIZE;
    float rotation : ROTATION;
    float subImageIndex : SUBIMAGE_INDEX;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD0;
    float2 texcoord2 : TEXCOORD1;
};

struct PS_Input_BeamTrail
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR;
};

PS_Input_BeamTrail VS(VS_Input_BeamTrail input)
{
    PS_Input_BeamTrail output;
    output.position = mul(float4(input.position, 1.0f), mul(View, Projection));
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}

float4 PS(PS_Input_BeamTrail input) : SV_TARGET
{
    float4 col = BeamTrailTexture.Sample(LinearClampSampler, input.texcoord);
    col *= input.color;
    clip(col.a - 0.01f);
    return col;
}
