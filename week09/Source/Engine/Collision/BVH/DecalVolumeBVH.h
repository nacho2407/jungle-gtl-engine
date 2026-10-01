// 충돌/피킹 영역에서 공유되는 decal-volume BVH 타입과 인터페이스를 정의합니다.
#pragma once

#include "Collision/BVH/BVH.h"

class FDecalSceneProxy;

class FDecalVolumeBVH
{
public:
    static constexpr int32 ChildFanout = 8;
    static constexpr int32 MaxLeafSize = 8;

    struct FLeaf
    {
        FBoundingBox Bounds;
        const FDecalSceneProxy* DecalProxy = nullptr;
        uint32 DecalIndex = 0;
    };

    using FBVH = TBVH<FLeaf, ChildFanout, MaxLeafSize>;
    using FNode = FBVH::FNode;

    void Build(const TArray<const FDecalSceneProxy*>& Decals);
    void QueryAABB(const FBoundingBox& ReceiverBounds, TArray<uint32>& OutDecalIndices) const;

    const TArray<FLeaf>& GetLeaves() const { return Leaves; }
    const TArray<FNode>& GetNodes() const { return BVH.GetNodes(); }

private:
    void QueryNodeRecursive(int32 NodeIndex, const FBoundingBox& ReceiverBounds, TArray<uint32>& OutDecalIndices) const;

    TArray<FLeaf> Leaves;
    FBVH BVH;
};
