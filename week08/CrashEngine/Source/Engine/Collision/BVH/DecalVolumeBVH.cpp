// 충돌/피킹 영역의 decal-volume BVH 세부 동작을 구현합니다.
#include "Collision/BVH/DecalVolumeBVH.h"

#include "Math/Intersection.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"

void FDecalVolumeBVH::Build(const TArray<const FDecalSceneProxy*>& Decals)
{
    Leaves.clear();
    BVH.Reset();

    Leaves.reserve(Decals.size());
    for (uint32 DecalIndex = 0; DecalIndex < static_cast<uint32>(Decals.size()); ++DecalIndex)
    {
        const FDecalSceneProxy* DecalProxy = Decals[DecalIndex];
        if (!DecalProxy || !DecalProxy->CachedBounds.IsValid())
        {
            continue;
        }

        FLeaf& Leaf = Leaves.emplace_back();
        Leaf.Bounds = DecalProxy->CachedBounds;
        Leaf.DecalProxy = DecalProxy;
        Leaf.DecalIndex = DecalIndex;
    }

    if (!Leaves.empty())
    {
        BVH.Build(
            Leaves,
            [](const FLeaf& Leaf) { return Leaf.Bounds; },
            [](const FLeaf& Leaf) { return Leaf.DecalIndex; });
    }
}

void FDecalVolumeBVH::QueryAABB(const FBoundingBox& ReceiverBounds, TArray<uint32>& OutDecalIndices) const
{
    if (!ReceiverBounds.IsValid() || BVH.GetNodes().empty())
    {
        return;
    }

    QueryNodeRecursive(0, ReceiverBounds, OutDecalIndices);
}

void FDecalVolumeBVH::QueryNodeRecursive(int32 NodeIndex, const FBoundingBox& ReceiverBounds, TArray<uint32>& OutDecalIndices) const
{
    const TArray<FNode>& Nodes = BVH.GetNodes();
    if (NodeIndex < 0 || NodeIndex >= static_cast<int32>(Nodes.size()))
    {
        return;
    }

    const FNode& Node = Nodes[NodeIndex];
    if (!FMath::IntersectAABB(ReceiverBounds, Node.Bounds))
    {
        return;
    }

    if (Node.IsLeaf())
    {
        for (int32 LeafIndex = 0; LeafIndex < Node.LeafCount; ++LeafIndex)
        {
            const FLeaf& Leaf = Leaves[Node.FirstLeaf + LeafIndex];
            if (Leaf.DecalProxy && Leaf.DecalProxy->GetDecalOBB().IntersectOBBAABB(ReceiverBounds))
            {
                OutDecalIndices.push_back(Leaf.DecalIndex);
            }
        }
        return;
    }

    for (int32 ChildIndex = 0; ChildIndex < Node.ChildCount; ++ChildIndex)
    {
        QueryNodeRecursive(Node.Children[ChildIndex], ReceiverBounds, OutDecalIndices);
    }
}
