// SceneDepth.hlsl
#include "../Common/Utils/Functions.hlsl"
#include "../Common/Resources/SystemResources.hlsl"

// b2 (PerShader0): SceneDepth visualization
cbuffer SceneDepthCB : register(b2)
{
    float Exponent;
    float NearClip;
    float FarClip;
    float Range;
    uint Mode;
    float3 _Padding;
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    
    // Reversed-Z: Near = 1.0, Far = 0.0
    float d = SceneDepth.Load(int3(coord, 0)).r;
    
    // 배경 (d=0) 처리: 물체가 없는 곳은 순수한 검은색
    if (d <= 0.0000001f)
    {
        return float4(0, 0, 0, 1);
    }

    // 실제 거리 계산 (NearClip 유닛 ~ FarClip 유닛)
    float linearZ = (NearClip * FarClip) / (d * (FarClip - NearClip) + NearClip);
    float v = 0.0f;
    
    if (Mode == 1) // Linear Mode (Repeating Sawtooth)
    {
        // Range 단위로 명암 반복 (0 -> 1 -> 0 ...)
        v = frac((linearZ - NearClip) / max(0.1f, Range));
        // Exponent를 통해 반복되는 명암의 대비(Contrast) 조절 가능
        v = pow(v, Exponent);
    }
    else // Power Mode (Smooth Transition)
    {
        // frac 없이 Range 거리까지 부드럽게 0에서 1로 변함
        v = saturate((linearZ - NearClip) / max(0.1f, Range));
        // Exponent를 통해 거리별 감쇄율 조절
        v = pow(v, Exponent);
    }
    
    return float4(v.xxx, 1.0f);
}
