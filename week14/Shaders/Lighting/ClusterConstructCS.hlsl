#include "Common/ConstantBuffers.hlsli"
#include "Common/ForwardLightData.hlsli"

RWStructuredBuffer<FAABB> gClusterAABBs : register(u0);

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

[numthreads(8, 3, 4)]
void CSMain(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    uint3 clusterCoord = groupId * uint3(8, 3, 4) + groupThreadId;
    if (clusterCoord.x >= CullState.ClusterX || clusterCoord.y >= CullState.ClusterY || clusterCoord.z >= CullState.ClusterZ)
    {
        return;
    }

    uint tileX = clusterCoord.x;
    uint tileY = clusterCoord.y;
    uint sliceZ = clusterCoord.z;
    //Get Clusters NDCs X,Y Size
    float2 tileSize = float2(2.0f / CullState.ClusterX, 2.0f / CullState.ClusterY);

    // SV_Position.y starts at the top of the viewport, where NDC.y is +1.
    float2 ndcMin = float2(-1.0f + tileX * tileSize.x, 1.0f - (tileY + 1) * tileSize.y);
    float2 ndcMax = float2(ndcMin.x + tileSize.x, 1.0f - tileY * tileSize.y);

    float nearZ = SliceToViewDepth(sliceZ);
    float farZ = SliceToViewDepth(sliceZ + 1);

    float3 corners[8];
    corners[0] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), nearZ);
    corners[1] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), nearZ);
    corners[2] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), nearZ);
    corners[3] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), nearZ);
    corners[4] = NDCToViewSpace(float2(ndcMin.x, ndcMin.y), farZ);
    corners[5] = NDCToViewSpace(float2(ndcMax.x, ndcMin.y), farZ);
    corners[6] = NDCToViewSpace(float2(ndcMin.x, ndcMax.y), farZ);
    corners[7] = NDCToViewSpace(float2(ndcMax.x, ndcMax.y), farZ);

    float3 aabbMin = 1e30f;
    float3 aabbMax = -1e30f;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        aabbMin = min(aabbMin, corners[i]);
        aabbMax = max(aabbMax, corners[i]);
    }

    uint clusterIdx = sliceZ * CullState.ClusterX * CullState.ClusterY
                    + tileY * CullState.ClusterX
                    + tileX;

    gClusterAABBs[clusterIdx].minPt = float4(aabbMin, 0.0f);
    gClusterAABBs[clusterIdx].maxPt = float4(aabbMax, 0.0f);
}
