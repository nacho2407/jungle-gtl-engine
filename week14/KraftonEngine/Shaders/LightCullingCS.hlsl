#include "Common/ConstantBuffers.hlsli"
#include "Common/ForwardLightData.hlsli"

StructuredBuffer<FAABB> gClusterAABBs : register(t0);
StructuredBuffer<FLightInfo> gLights : register(t1);

// 출력: 각 클러스터가 가진 광원 인덱스들의 압축 리스트
RWStructuredBuffer<uint> gLightIndexList : register(u1);
// 출력: 각 클러스터의 (offset, count)
RWStructuredBuffer<uint2> gLightGrid : register(u2);
// 전역 카운터 (atomic 사용)
RWStructuredBuffer<uint> gGlobalCounter : register(u3);
bool SphereIntersectsAABB(float3 center, float radius, FAABB aabb)
{
    float3 closest = clamp(center, aabb.minPt.xyz, aabb.maxPt.xyz);
    float3 diff = closest - center;
    return dot(diff, diff) <= (radius * radius);
}

// 클러스터 하나당 스레드 1개
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint clusterIdx = id.z * CullState.ClusterX * CullState.ClusterY
                    + id.y * CullState.ClusterX
                    + id.x;

    FAABB clusterAABB = gClusterAABBs[clusterIdx];

    // 임시 버퍼 (로컬 광원 인덱스 리스트)
    uint localLightIndices[128];
    uint localCount = 0;

    // 모든 광원과 교차 테스트
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; i++)
    {
        FLightInfo light = gLights[i];
        float4 LightPos4 = float4(gLights[i].Position, 1);
        float4 ViewPos = mul(LightPos4, View);
        if (SphereIntersectsAABB(ViewPos.xyz, light.AttenuationRadius, clusterAABB))
        {
            if (localCount < 128)
                localLightIndices[localCount++] = i;
        }
    }

    // 전역 리스트에 atomic으로 공간 예약
    uint offset;
    InterlockedAdd(gGlobalCounter[0], localCount, offset);

    // Light Grid에 offset/count 기록
    gLightGrid[clusterIdx] = uint2(offset, localCount);

    // Light Index List에 복사
    for (uint j = 0; j < localCount; j++)
    {
        gLightIndexList[offset + j] = localLightIndices[j];
    }
}
