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

// SceneDepth (t10) is declared in Common/ConstantBuffers.hlsl

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);
    
    float d = SceneDepth.Load(int3(coord, 0));
    
    float v = 0.0f;
    
    if (Mode == 1)
    {
        // Reversed-Z linearization: d=1 at near, d=0 at far
        float linZ = NearClip * FarClip / (NearClip - d * (NearClip - FarClip));

        // 전체 far clip 기준으로 정규화하면 원거리 카메라에서 화면이 거의 검게 뭉개진다.
        // UI의 Range 값을 시각화 상한으로 사용해 근거리 깊이 분포를 더 잘 보이게 한다.
        float visFar = max(Range, NearClip + 0.001f);
        v = saturate((linZ - NearClip) / (visFar - NearClip));
    }
    else
    {
        // Reversed-Z: invert so near=dark, far=bright (matching Forward-Z visual)
        v = pow(saturate(1.0 - d), Exponent);
    }

    // UE-style repeated checker toward the bright/far range to improve depth readability.
    int2 checkerCoord = coord / 8;
    float checker = ((checkerCoord.x + checkerCoord.y) & 1) ? 0.82f : 1.0f;
    float checkerStrength = smoothstep(0.55f, 0.98f, v);
    v *= lerp(1.0f, checker, checkerStrength);
    
    return float4(v, v, v, 1.0f);
}
