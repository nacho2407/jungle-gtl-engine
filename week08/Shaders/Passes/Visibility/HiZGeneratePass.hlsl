/*
    HiZGeneratePass.hlsl는 컬링/가시성 계산에 쓰는 셰이더입니다.

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
    - 이 파일에서 직접 선언한 슬롯: b0, t0, u0
*/

// Hi-Z Mip Chain Generator
// CSCopyDepth: depth buffer to Hi-Z mip 0.
// CSDownsample: Hi-Z mip N-1 to Hi-Z mip N.

cbuffer HiZParams : register(b0)
{
    uint2 SrcDimensions;
    uint2 _pad;
};

Texture2D<float> SrcTexture : register(t0);
RWTexture2D<float> DstTexture : register(u0);

[numthreads(8, 8, 1)]
// 병렬 스레드 그룹으로 실행되는 컴퓨트 셰이더입니다.
void CSCopyDepth(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= SrcDimensions.x || DTid.y >= SrcDimensions.y)
        return;

    DstTexture[DTid.xy] = SrcTexture[DTid.xy];
}

[numthreads(8, 8, 1)]
// 병렬 스레드 그룹으로 실행되는 컴퓨트 셰이더입니다.
void CSDownsample(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dstSize = max(SrcDimensions / 2, uint2(1, 1));

    if (DTid.x >= dstSize.x || DTid.y >= dstSize.y)
        return;

    uint2 srcCoord = DTid.xy * 2;
    uint2 srcMax   = SrcDimensions - uint2(1, 1);

    float d00 = SrcTexture[srcCoord];
    float d10 = SrcTexture[min(srcCoord + uint2(1, 0), srcMax)];
    float d01 = SrcTexture[min(srcCoord + uint2(0, 1), srcMax)];
    float d11 = SrcTexture[min(srcCoord + uint2(1, 1), srcMax)];

    // Reversed-Z: keep the farthest (smallest) depth for conservative occlusion
    DstTexture[DTid.xy] = min(min(d00, d10), min(d01, d11));
}


