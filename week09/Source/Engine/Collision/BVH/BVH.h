// 충돌/피킹 영역에서 공유되는 범용 BVH 빌더입니다.
#pragma once

#include <algorithm>
#include <cfloat>

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"

template <typename TLeaf, int32 Fanout, int32 MaxLeafSize>
class TBVH
{
public:
    struct FNode
    {
        FBoundingBox Bounds;
        int32 Children[Fanout] = {};

        alignas(32) float ChildMinX[Fanout];
        alignas(32) float ChildMinY[Fanout];
        alignas(32) float ChildMinZ[Fanout];
        alignas(32) float ChildMaxX[Fanout];
        alignas(32) float ChildMaxY[Fanout];
        alignas(32) float ChildMaxZ[Fanout];

        int32 ChildCount = 0;
        int32 FirstLeaf = 0;
        int32 LeafCount = 0;
        int32 PayloadIndex = -1;
        int32 PayloadCount = 0;

        bool IsLeaf() const { return ChildCount == 0; }
    };

    void Reset()
    {
        Nodes.clear();
    }

    template <typename TBoundsGetter, typename TStableKeyGetter>
    void Build(TArray<TLeaf>& Leaves, TBoundsGetter GetBounds, TStableKeyGetter GetStableKey)
    {
        Nodes.clear();
        if (!Leaves.empty())
        {
            BuildRecursive(Leaves, 0, static_cast<int32>(Leaves.size()), GetBounds, GetStableKey);
        }
    }

    const TArray<FNode>& GetNodes() const { return Nodes; }

private:
    template <typename TVectorLike>
    static float GetAxisComponent(const TVectorLike& Vector, int32 Axis)
    {
        return Axis == 0 ? Vector.X : (Axis == 1 ? Vector.Y : Vector.Z);
    }

    static float GetBoundsSurfaceArea(const FBoundingBox& Bounds)
    {
        const FVector Extent = Bounds.GetExtent();
        const float Width = (std::max)(Extent.X * 2.0f, 0.0f);
        const float Height = (std::max)(Extent.Y * 2.0f, 0.0f);
        const float Depth = (std::max)(Extent.Z * 2.0f, 0.0f);
        return 2.0f * ((Width * Height) + (Width * Depth) + (Height * Depth));
    }

    void FillUnusedChildBounds(FNode& Node)
    {
        for (int32 Index = Node.ChildCount; Index < Fanout; ++Index)
        {
            Node.Children[Index] = -1;
            Node.ChildMinX[Index] = 1e30f;
            Node.ChildMinY[Index] = 1e30f;
            Node.ChildMinZ[Index] = 1e30f;
            Node.ChildMaxX[Index] = -1e30f;
            Node.ChildMaxY[Index] = -1e30f;
            Node.ChildMaxZ[Index] = -1e30f;
        }
    }

    void CopyChildBounds(FNode& Node, int32 ChildSlot, const FBoundingBox& ChildBounds)
    {
        Node.ChildMinX[ChildSlot] = ChildBounds.Min.X;
        Node.ChildMinY[ChildSlot] = ChildBounds.Min.Y;
        Node.ChildMinZ[ChildSlot] = ChildBounds.Min.Z;
        Node.ChildMaxX[ChildSlot] = ChildBounds.Max.X;
        Node.ChildMaxY[ChildSlot] = ChildBounds.Max.Y;
        Node.ChildMaxZ[ChildSlot] = ChildBounds.Max.Z;
    }

    template <typename TBoundsGetter, typename TStableKeyGetter>
    int32 BuildRecursive(TArray<TLeaf>& Leaves, int32 Start, int32 End, TBoundsGetter GetBounds, TStableKeyGetter GetStableKey)
    {
        const int32 NodeIndex = static_cast<int32>(Nodes.size());
        Nodes.emplace_back();

        FBoundingBox Bounds;
        for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
        {
            const FBoundingBox LeafBounds = GetBounds(Leaves[LeafIndex]);
            Bounds.Expand(LeafBounds.Min);
            Bounds.Expand(LeafBounds.Max);
        }
        Nodes[NodeIndex].Bounds = Bounds;

        const int32 LeafCount = End - Start;
        if (LeafCount <= MaxLeafSize)
        {
            FNode& LeafNode = Nodes[NodeIndex];
            LeafNode.FirstLeaf = Start;
            LeafNode.LeafCount = LeafCount;
            FillUnusedChildBounds(LeafNode);
            return NodeIndex;
        }

        float BestCost = FLT_MAX;
        int32 BestAxis = 0;
        bool bFoundValidAxis = false;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            float MinCenter = FLT_MAX;
            float MaxCenter = -FLT_MAX;

            for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
            {
                const float Center = GetAxisComponent(GetBounds(Leaves[LeafIndex]).GetCenter(), Axis);
                MinCenter = (std::min)(MinCenter, Center);
                MaxCenter = (std::max)(MaxCenter, Center);
            }

            if (MaxCenter - MinCenter <= 1e-4f)
            {
                continue;
            }

            FBoundingBox BucketBounds[Fanout];
            int32 BucketCounts[Fanout] = {};
            const float Scale = static_cast<float>(Fanout) / (MaxCenter - MinCenter);

            for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
            {
                const FBoundingBox LeafBounds = GetBounds(Leaves[LeafIndex]);
                const float Center = GetAxisComponent(LeafBounds.GetCenter(), Axis);
                int32 Bucket = static_cast<int32>((Center - MinCenter) * Scale);
                Bucket = std::clamp(Bucket, 0, Fanout - 1);
                BucketBounds[Bucket].Expand(LeafBounds.Min);
                BucketBounds[Bucket].Expand(LeafBounds.Max);
                BucketCounts[Bucket]++;
            }

            float AxisCost = 0.0f;
            for (int32 Bucket = 0; Bucket < Fanout; ++Bucket)
            {
                if (BucketCounts[Bucket] == 0)
                {
                    continue;
                }
                AxisCost += GetBoundsSurfaceArea(BucketBounds[Bucket]) * static_cast<float>(BucketCounts[Bucket]);
            }

            if (AxisCost < BestCost)
            {
                BestCost = AxisCost;
                BestAxis = Axis;
                bFoundValidAxis = true;
            }
        }

        auto AddChildRange = [&](int32 RangeStart, int32 RangeEnd)
        {
            if (RangeStart >= RangeEnd)
            {
                return;
            }

            const int32 ChildIndex = BuildRecursive(Leaves, RangeStart, RangeEnd, GetBounds, GetStableKey);
            FNode& ParentNode = Nodes[NodeIndex];
            const int32 LocalChildSlot = ParentNode.ChildCount;
            ParentNode.Children[LocalChildSlot] = ChildIndex;
            CopyChildBounds(ParentNode, LocalChildSlot, Nodes[ChildIndex].Bounds);
            ParentNode.ChildCount++;
        };

        if (!bFoundValidAxis)
        {
            const FBoundingBox ReferenceBounds = GetBounds(Leaves[Start]);
            const FVector Extent = ReferenceBounds.GetExtent();
            BestAxis = (Extent.Y > Extent.X && Extent.Y >= Extent.Z) ? 1 : ((Extent.Z > Extent.X && Extent.Z > Extent.Y) ? 2 : 0);

            std::sort(
                Leaves.begin() + Start,
                Leaves.begin() + End,
                [&](const TLeaf& A, const TLeaf& B)
                {
                    const float CenterA = GetAxisComponent(GetBounds(A).GetCenter(), BestAxis);
                    const float CenterB = GetAxisComponent(GetBounds(B).GetCenter(), BestAxis);
                    if (CenterA != CenterB)
                    {
                        return CenterA < CenterB;
                    }
                    return GetStableKey(A) < GetStableKey(B);
                });

            const int32 DesiredChildren = (std::min<int32>)(Fanout, LeafCount);
            for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
            {
                const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
                const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
                AddChildRange(RangeStart, RangeEnd);
            }
        }
        else
        {
            float MinCenter = FLT_MAX;
            float MaxCenter = -FLT_MAX;
            for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
            {
                const float Center = GetAxisComponent(GetBounds(Leaves[LeafIndex]).GetCenter(), BestAxis);
                MinCenter = (std::min)(MinCenter, Center);
                MaxCenter = (std::max)(MaxCenter, Center);
            }

            const float Scale = static_cast<float>(Fanout) / (MaxCenter - MinCenter);
            auto GetBucket = [&](const TLeaf& Leaf)
            {
                const float Center = GetAxisComponent(GetBounds(Leaf).GetCenter(), BestAxis);
                int32 Bucket = static_cast<int32>((Center - MinCenter) * Scale);
                return std::clamp(Bucket, 0, Fanout - 1);
            };

            std::sort(
                Leaves.begin() + Start,
                Leaves.begin() + End,
                [&](const TLeaf& A, const TLeaf& B)
                {
                    const int32 BucketA = GetBucket(A);
                    const int32 BucketB = GetBucket(B);
                    if (BucketA != BucketB)
                    {
                        return BucketA < BucketB;
                    }

                    const float CenterA = GetAxisComponent(GetBounds(A).GetCenter(), BestAxis);
                    const float CenterB = GetAxisComponent(GetBounds(B).GetCenter(), BestAxis);
                    if (CenterA != CenterB)
                    {
                        return CenterA < CenterB;
                    }

                    return GetStableKey(A) < GetStableKey(B);
                });

            if (GetBucket(Leaves[Start]) == GetBucket(Leaves[End - 1]))
            {
                const int32 DesiredChildren = (std::min<int32>)(Fanout, LeafCount);
                for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
                {
                    const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
                    const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
                    AddChildRange(RangeStart, RangeEnd);
                }
            }
            else
            {
                int32 RangeStart = Start;
                while (RangeStart < End)
                {
                    int32 RangeEnd = RangeStart + 1;
                    const int32 Bucket = GetBucket(Leaves[RangeStart]);
                    while (RangeEnd < End && GetBucket(Leaves[RangeEnd]) == Bucket)
                    {
                        ++RangeEnd;
                    }

                    AddChildRange(RangeStart, RangeEnd);
                    RangeStart = RangeEnd;
                }
            }
        }

        FillUnusedChildBounds(Nodes[NodeIndex]);
        return NodeIndex;
    }

private:
    TArray<FNode> Nodes;
};
