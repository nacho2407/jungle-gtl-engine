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
float SliceToViewDepth(uint zSlice)
{
    return CullState.NearZ * pow(CullState.FarZ / CullState.NearZ, (float) zSlice / CullState.ClusterZ);
}

float3 NDCToViewSpace(float2 ndc, float viewDepth)
{
    float4 clipPos = float4(ndc.x, ndc.y, 1.0f, 1.0f);
    float4 viewPos = mul(clipPos, InvProj);
    viewPos /= viewPos.w;
    return viewPos.xyz / viewPos.z * viewDepth;
}

float4 MakePlane(float3 a, float3 b, float3 c, float3 insidePoint)
{
    float3 n = normalize(cross(b - a, c - a));
    float d = -dot(n, a);
    if (dot(n, insidePoint) + d < 0.0f)
    {
        n = -n;
        d = -d;
    }
    return float4(n, d);
}

bool SphereInsidePlane(float3 center, float radius, float4 plane)
{
    return dot(plane.xyz, center) + plane.w >= -radius;
}

bool SphereOverlapsCluster(float3 center, float radius, float3 corners[8])
{
    float3 insidePoint = 0.0f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        insidePoint += corners[i];
    }
    insidePoint *= 0.125f;

    float4 planes[6];
    planes[0] = MakePlane(corners[0], corners[1], corners[2], insidePoint); // near
    planes[1] = MakePlane(corners[4], corners[6], corners[5], insidePoint); // far
    planes[2] = MakePlane(corners[0], corners[2], corners[4], insidePoint); // left
    planes[3] = MakePlane(corners[1], corners[5], corners[3], insidePoint); // right
    planes[4] = MakePlane(corners[0], corners[4], corners[1], insidePoint); // bottom
    planes[5] = MakePlane(corners[2], corners[3], corners[6], insidePoint); // top

    [unroll]
    for (int p = 0; p < 6; ++p)
    {
        if (!SphereInsidePlane(center, radius, planes[p]))
        {
            return false;
        }
    }

    return true;
}

// 클러스터 하나당 스레드 1개
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint clusterIdx = id.z * CullState.ClusterX * CullState.ClusterY
                    + id.y * CullState.ClusterX
                    + id.x;

    float2 tileSize = float2(2.0f / CullState.ClusterX, 2.0f / CullState.ClusterY);
    float2 ndcMin = float2(-1.0f + id.x * tileSize.x, 1.0f - (id.y + 1) * tileSize.y);
    float2 ndcMax = float2(ndcMin.x + tileSize.x, 1.0f - id.y * tileSize.y);
    float nearZ = SliceToViewDepth(id.z);
    float farZ = SliceToViewDepth(id.z + 1);

    float3 corners[8];
    corners[0] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), nearZ);
    corners[1] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), nearZ);
    corners[2] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), nearZ);
    corners[3] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), nearZ);
    corners[4] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), farZ);
    corners[5] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), farZ);
    corners[6] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), farZ);
    corners[7] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), farZ);

    // 임시 버퍼 (로컬 광원 인덱스 리스트)
    uint localLightIndices[128];
    uint localCount = 0;

    // 모든 광원과 교차 테스트
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; i++)
    {
        FLightInfo light = gLights[i];
        float4 LightPos4 = float4(gLights[i].Position, 1);
        float4 ViewPos = mul(LightPos4, View);
        if (SphereOverlapsCluster(ViewPos.xyz, light.AttenuationRadius, corners))
        {
            if (localCount < min(CullState.MaxLightsPerCluster, 128))
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
