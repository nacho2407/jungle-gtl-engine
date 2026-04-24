
/*
    LightHitMapPass.hlsl는 후처리 렌더 패스의 셰이더입니다.

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
    - 이 파일에서 직접 선언한 슬롯: t10, t8
*/

// Fullscreen Triangle VS + light-hit debug PS.

#include "../../Common/Utils/Functions.hlsl"
#include "../../Common/Resources/SystemResources.hlsl"
#include "../../Common/Resources/SystemSamplers.hlsl"

//Texture2D<float>  SceneDepth  : register(t10);  // SystemResources.hlsl
Texture2D g_DebugHitMapTex : register(t8);

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    int2 coord = int2(input.position.xy);

    float4 BaseColor = SceneColor.Load(int3(coord, 0));
    return BaseColor + g_DebugHitMapTex.Sample(PointClampSampler, input.uv);
}

