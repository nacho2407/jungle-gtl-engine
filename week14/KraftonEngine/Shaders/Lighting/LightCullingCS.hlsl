#include "Common/ConstantBuffers.hlsli"
#include "Common/ForwardLightData.hlsli"

StructuredBuffer<FAABB> gClusterAABBs : register(t0);
StructuredBuffer<FLightInfo> gLights : register(t1);

RWStructuredBuffer<uint> gLightIndexList : register(u1);
RWStructuredBuffer<uint2> gLightGrid : register(u2);
RWStructuredBuffer<uint> gGlobalCounter : register(u3);

#define CLUSTER_LIGHT_CULLING_THREADS 256

// One thread group owns one cluster. These values are shared by the 256 threads
// inside that group while they split the light loop.
groupshared uint gsLightIndices[256];
groupshared uint gsHitCount;
groupshared float4 gsClusterPlanes[6];

float SliceToViewDepth(uint zSlice)
{
    if (CullState.bIsOrtho > 0)
    {
        return CullState.NearZ + (CullState.FarZ - CullState.NearZ) * ((float) zSlice / CullState.ClusterZ);
    }
    else
    {
        return CullState.NearZ * pow(CullState.FarZ / CullState.NearZ, (float) zSlice / CullState.ClusterZ);
    }
}

float3 NDCToViewSpace(float2 ndc, float viewDepth)
{
    if (CullState.bIsOrtho > 0)
    {
        float aspect = (float) CullState.ScreenWidth / (float) CullState.ScreenHeight;
        float orthoHeight = CullState.OrthoWidth / aspect;
        return float3(ndc.x * CullState.OrthoWidth * 0.5f, ndc.y * orthoHeight * 0.5f, viewDepth);
    }
    else
    {
        float4 clipPos = float4(ndc.x, ndc.y, 1.0f, 1.0f);
        float4 viewPos = mul(clipPos, InvProj);
        viewPos /= viewPos.w;
        return viewPos.xyz / viewPos.z * viewDepth;
    }
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

void BuildClusterPlanes(float3 corners[8], out float4 planes[6])
{
    float3 insidePoint = 0.0f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        insidePoint += corners[i];
    }
    insidePoint *= 0.125f;

    planes[0] = MakePlane(corners[0], corners[1], corners[2], insidePoint); // near
    planes[1] = MakePlane(corners[4], corners[6], corners[5], insidePoint); // far
    planes[2] = MakePlane(corners[0], corners[2], corners[4], insidePoint); // left
    planes[3] = MakePlane(corners[1], corners[5], corners[3], insidePoint); // right
    planes[4] = MakePlane(corners[0], corners[4], corners[1], insidePoint); // bottom
    planes[5] = MakePlane(corners[2], corners[3], corners[6], insidePoint); // top
}

bool SphereOverlapsClusterPlanes(float3 center, float radius)
{
    [unroll]
    for (int p = 0; p < 6; ++p)
    {
        if (!SphereInsidePlane(center, radius, gsClusterPlanes[p]))
        {
            return false;
        }
    }

    return true;
}

void BuildCurrentClusterPlanes(uint clusterIdx)
{
    uint sliceSize = CullState.ClusterX * CullState.ClusterY;
    uint clusterZ = clusterIdx / sliceSize;
    uint clusterXY = clusterIdx - clusterZ * sliceSize;
    uint clusterY = clusterXY / CullState.ClusterX;
    uint clusterX = clusterXY - clusterY * CullState.ClusterX;
    uint3 clusterCoord = uint3(clusterX, clusterY, clusterZ);

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

    float4 planes[6];
    BuildClusterPlanes(corners, planes);

    [unroll]
    for (int p = 0; p < 6; ++p)
    {
        gsClusterPlanes[p] = planes[p];
    }
}

// One dispatch group maps to one cluster.
// The 256 threads in that group divide the light list with i += 256.
[numthreads(CLUSTER_LIGHT_CULLING_THREADS, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    uint clusterCount = CullState.ClusterX * CullState.ClusterY * CullState.ClusterZ;
    uint clusterIdx = groupId.x;
    uint threadIndex = groupThreadId.x;
    uint localCapacity = min(CullState.MaxLightsPerCluster, 256);

    if (clusterIdx >= clusterCount)
    {
        return;
    }

    // Thread 0 prepares the cluster data once. The rest of the group only tests
    // lights against the shared planes, so plane construction is not repeated per light.
    if (threadIndex == 0)
    {
        gsHitCount = 0;
        BuildCurrentClusterPlanes(clusterIdx);
    }
    GroupMemoryBarrierWithGroupSync();

    // Split the light loop across the group. With 720 lights, each thread tests
    // roughly 3 lights instead of one thread testing all 720 lights by itself.
    uint totalLights = NumActivePointLights + NumActiveSpotLights;
    if (localCapacity > 0)
    {
        for (uint i = threadIndex; i < totalLights; i += CLUSTER_LIGHT_CULLING_THREADS)
        {
            FLightInfo light = gLights[i];
            float4 viewPos = mul(float4(light.Position, 1.0f), View);
            if (SphereOverlapsClusterPlanes(viewPos.xyz, light.AttenuationRadius))
            {
                uint slot;
                InterlockedAdd(gsHitCount, 1, slot);

                // Many threads can hit at the same time. InterlockedAdd gives each
                // hit a unique slot, and the capacity check truncates overflow.
                if (slot < localCapacity)
                {
                    gsLightIndices[slot] = i;
                }
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // Only one thread publishes the compacted list to the global buffers.
    if (threadIndex == 0)
    {
        uint localCount = min(gsHitCount, localCapacity);
        uint offset;
        InterlockedAdd(gGlobalCounter[0], localCount, offset);

        gLightGrid[clusterIdx] = uint2(offset, localCount);

        for (uint j = 0; j < localCount; j++)
        {
            gLightIndexList[offset + j] = gsLightIndices[j];
        }
    }
}
