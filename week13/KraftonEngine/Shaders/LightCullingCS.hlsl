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

float ComputeLightImportance(FLightInfo light, float3 viewLightPos, float3 clusterCenter)
{
    float dist = length(viewLightPos - clusterCenter);
    float ratio = saturate(dist / max(light.AttenuationRadius, 0.0001f));
    float attenuation = pow(1.0f - ratio, light.FalloffExponent);
    float luminance = dot(light.Color.rgb, float3(0.299f, 0.587f, 0.114f));
    return luminance * light.Intensity * attenuation;
}

//하나의 group당 4^3의 스레드
[numthreads(8, 3, 4)]
void CSMain(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    uint3 clusterCoord = groupId * uint3(8, 3, 4) + groupThreadId;
    if (clusterCoord.x >= CullState.ClusterX || clusterCoord.y >= CullState.ClusterY || clusterCoord.z >= CullState.ClusterZ)
    {
        return;
    }

    uint clusterIdx = clusterCoord.z * CullState.ClusterX * CullState.ClusterY
                    + clusterCoord.y * CullState.ClusterX
                    + clusterCoord.x;

    float2 tileSize = float2(2.0f / CullState.ClusterX, 2.0f / CullState.ClusterY);
    float2 ndcMin = float2(-1.0f + clusterCoord.x * tileSize.x, 1.0f - (clusterCoord.y + 1) * tileSize.y);
    float2 ndcMax = float2(ndcMin.x + tileSize.x, 1.0f - clusterCoord.y * tileSize.y);
    float nearZ = SliceToViewDepth(clusterCoord.z);
    float farZ = SliceToViewDepth(clusterCoord.z + 1);

    float3 corners[8];
    corners[0] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), nearZ);
    corners[1] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), nearZ);
    corners[2] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), nearZ);
    corners[3] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), nearZ);
    corners[4] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), farZ);
    corners[5] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), farZ);
    corners[6] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), farZ);
    corners[7] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), farZ);

    float3 clusterCenter = 0.0f;
    [unroll]
    for (int c = 0; c < 8; ++c)
    {
        clusterCenter += corners[c];
    }
    clusterCenter *= 0.125f;

    // 임시 버퍼 (로컬 광원 인덱스 리스트)
    uint localLightIndices[256];
    float localLightScores[256];
    uint localCount = 0;
    uint localCapacity = min(CullState.MaxLightsPerCluster, 256);
    uint minIndex = 0;
    float minScore = 3.402823e38f;

    // 모든 광원과 교차 테스트
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; i++)
    {
        FLightInfo light = gLights[i];
        float4 LightPos4 = float4(gLights[i].Position, 1);
        float4 ViewPos = mul(LightPos4, View);
        if (SphereOverlapsCluster(ViewPos.xyz, light.AttenuationRadius, corners))
        {
            float importance = ComputeLightImportance(light, ViewPos.xyz, clusterCenter);
            if (localCount < localCapacity)
            {
                localLightIndices[localCount++] = i;
                localLightScores[localCount - 1] = importance;
                if (importance < minScore)
                {
                    minScore = importance;
                    minIndex = localCount - 1;
                }
            }
            else if (localCapacity > 0)
            {
                if (importance > minScore)
                {
                    localLightIndices[minIndex] = i;
                    localLightScores[minIndex] = importance;

                    minIndex = 0;
                    minScore = localLightScores[0];
                    for (uint s = 1; s < localCapacity; ++s)
                    {
                        if (localLightScores[s] < minScore)
                        {
                            minScore = localLightScores[s];
                            minIndex = s;
                        }
                    }
                }
            }
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
