
/*
    ClusterBinningPassCS.hlsl는 컬링/가시성 계산에 쓰는 셰이더입니다.

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
    - 이 파일에서 직접 선언한 슬롯: u0
*/

#include "../../Common/Resources/ConstantBuffers.hlsl"

RWStructuredBuffer<uint> g_ClusterBins : register(u0);

[numthreads(8, 8, 1)]
// 병렬 스레드 그룹으로 실행되는 컴퓨트 셰이더입니다.
void CS(uint3 DTid : SV_DispatchThreadID)
{
    const uint LinearIndex = DTid.x + DTid.y * 1024u;
    g_ClusterBins[LinearIndex] = 0u;
}

