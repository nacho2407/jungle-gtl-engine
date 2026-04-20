#include "Common/ConstantBuffers.hlsli"
#include "Common/ForwardLightData.hlsli"

RWStructuredBuffer<FAABB> gClusterAABBs : register(u0);

float3 NDCToViewSpace(float2 ndc, float viewDepth)
{
    float4 clipPos = float4(ndc.x, ndc.y, 1.0f, 1.0f);
    float4 viewPos = mul(clipPos, InvProj);
    viewPos /= viewPos.w;
    return viewPos.xyz / viewPos.z * viewDepth;
}

float SliceToViewDepth(uint zSlice)
{
    return CullState.NearZ * pow(CullState.FarZ / CullState.NearZ, (float) zSlice / CullState.ClusterZ);
}

[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint tileX = id.x;
    uint tileY = id.y;
    uint sliceZ = id.z;
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
