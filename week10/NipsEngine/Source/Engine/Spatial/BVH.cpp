#include "BVH.h"

#include <cassert>
#include <algorithm>
#include <cfloat>

#include "Engine/Geometry/Frustum.h"
#include "Engine/Geometry/AABB.h"

/**
 * @brief Build a BVH over the provided object bounds.
 * @param ObjectBounds Array of per-object AABBs.
 * @param InLeafSize Maximum number of objects per leaf node.
 */
void FBVH::BuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize)
{
    Reset();
    LeafSize = InLeafSize;

    const int32 ObjectCount = static_cast<int32>(ObjectBounds.size());
    if (ObjectCount == 0)
    {
        RootNodeIndex = INDEX_NONE;
        return;
    }

    ObjectIndices.resize(ObjectCount);
    for (int32 i{0}; i < ObjectCount; ++i)
    {
        ObjectIndices[i] = i;
    }
    RootNodeIndex = BuildNode(ObjectBounds, 0, ObjectCount);
}

void FBVH::BuildBLAS(const TArray<FAABB>& ObjectBounds, int32 InLeafSize)
{
    BuildBVH(ObjectBounds, InLeafSize);
}

void FBVH::RefitBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize) {}

void FBVH::ReBuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize) {}

void FBVH::FrustumQuery(const FFrustum& Frustum, TArray<uint32>& OutIndices, bool bInsideOnly) const
{
    if (RootNodeIndex == INDEX_NONE)
    {
        return;
    }

    struct FStackEntry
    {
        int32 NodeIndex;
        bool  bAssumeInside;
    };

    TArray<FStackEntry> Stack;
    Stack.reserve(Nodes.size());
    Stack.push_back({RootNodeIndex, false});

    while (!Stack.empty())
    {
        const FStackEntry Entry = Stack.back();
        Stack.pop_back();

        const FNode& Node = Nodes[Entry.NodeIndex];

        FFrustum::EFrustumIntersectResult Result = FFrustum::EFrustumIntersectResult::Intersect;
        if (!Entry.bAssumeInside)
        {
            Result = Frustum.Intersects(Node.Bounds);
            if (Result == FFrustum::EFrustumIntersectResult::Outside)
            {
                continue;
            }
            if (bInsideOnly && Result == FFrustum::EFrustumIntersectResult::Intersect)
            {
                continue;
            }
        }
        else
        {
            Result = FFrustum::EFrustumIntersectResult::Inside;
        }

        if (Node.IsLeaf())
        {
            for (int32 i = 0; i < Node.ObjectCount; ++i)
            {
                OutIndices.push_back(ObjectIndices[Node.FirstObject + i]);
            }
            continue;
        }

        const bool bChildAssumeInside =
            (Entry.bAssumeInside || Result == FFrustum::EFrustumIntersectResult::Inside);

        if (Node.Left != INDEX_NONE)
        {
            Stack.push_back({Node.Left, bChildAssumeInside});
        }
        if (Node.Right != INDEX_NONE)
        {
            Stack.push_back({Node.Right, bChildAssumeInside});
        }
    }
}

void FBVH::RayQuery(const TArray<FAABB>& ObjectBounds, const FRay& Ray, TArray<int32>& OutIndices,
                    TArray<float>& OutTs) const
{
    OutIndices.clear();
    OutTs.clear();

    if (RootNodeIndex == INDEX_NONE)
        return;

    struct FEntry
    {
        int32 NodeIndex;
        float tEnter;
    };

    TArray<FEntry> Stack;
    Stack.reserve(64);

    float tRoot = 0.f;
    if (!Nodes[RootNodeIndex].Bounds.IntersectRay(Ray, tRoot))
        return;
    Stack.push_back({RootNodeIndex, tRoot});

    while (!Stack.empty())
    {
        auto [NodeIndex, tEnter] = Stack.back();
        Stack.pop_back();

        const FNode& Node = Nodes[NodeIndex];

        if (Node.IsLeaf())
        {
            for (int32 idx = 0; idx < Node.ObjectCount; ++idx)
            {
                const int32 ObjIndex = ObjectIndices[Node.FirstObject + idx];
                float       HitT = 0.f;
                if (ObjectBounds[ObjIndex].IntersectRay(Ray, HitT) && HitT >= 0.f)
                {
                    OutIndices.push_back(ObjIndex);
                    OutTs.push_back(HitT);
                }
            }
            continue;
        }

        float      tL = FLT_MAX, tR = FLT_MAX;
        const bool bL = (Node.Left != INDEX_NONE) && Nodes[Node.Left].Bounds.IntersectRay(Ray, tL);
        const bool bR =
            (Node.Right != INDEX_NONE) && Nodes[Node.Right].Bounds.IntersectRay(Ray, tR);

        // 가까운 노드를 나중에 push → LIFO로 먼저 pop = Front-to-Back 순서 보장
        //  수집된 OutTs가 대체로 오름차순이므로 후처리 정렬 비용 절감
        if (bL && bR)
        {
            if (tL < tR)
            {
                Stack.push_back({Node.Right, tR});
                Stack.push_back({Node.Left, tL});
            }
            else
            {
                Stack.push_back({Node.Left, tL});
                Stack.push_back({Node.Right, tR});
            }
        }
        else if (bL)
            Stack.push_back({Node.Left, tL});
        else if (bR)
            Stack.push_back({Node.Right, tR});
    }

    if (OutIndices.size() <= 1)
        return; // 0~1개는 정렬 불필요

    // Front-to-Back 순회로 이미 대체로 정렬된 상태 → insertion sort가 평균적으로 더 빠름
    // std::sort 대신 std::stable_sort or 단순 insertion sort 고려 가능
    // 히트 수가 적을 경우(TLAS는 보통 수십 개 이하) 아래로 충분
    const int32   N = static_cast<int32>(OutIndices.size());
    TArray<int32> Order(N);
    for (int32 i = 0; i < N; ++i)
        Order[i] = i;

    std::sort(Order.begin(), Order.end(), [&](int32 A, int32 B) { return OutTs[A] < OutTs[B]; });

    TArray<int32> SortedIndices;
    SortedIndices.reserve(N);
    TArray<float> SortedTs;
    SortedTs.reserve(N);
    for (int32 Ord : Order)
    {
        SortedIndices.push_back(OutIndices[Ord]);
        SortedTs.push_back(OutTs[Ord]);
    }
    OutIndices.swap(SortedIndices);
    OutTs.swap(SortedTs);
}

void FBVH::RayQueryTriangle(const FRay& Ray, TArray<int32>& OutIndices) const
{
    if (RootNodeIndex == INDEX_NONE)
    {
        return;
    }

    struct FEntry
    {
        int32 NodeIndex;
        float tEnter;
    };

    TArray<FEntry> Stack;
    Stack.reserve(64);

    float tRoot = 0.0f;
    if (!Nodes[RootNodeIndex].Bounds.IntersectRay(Ray, tRoot))
        return;
    Stack.push_back({RootNodeIndex, tRoot});

    float tMax = FLT_MAX;

    while (!Stack.empty())
    {
        auto [NodeIndex, tEnter] = Stack.back();
        Stack.pop_back();

        if (tEnter >= tMax)
            continue;
        const FNode& Node = Nodes[NodeIndex];

        float      tL = FLT_MAX, tR = FLT_MAX;
        const bool bL = (Node.Left != INDEX_NONE) && Nodes[Node.Left].Bounds.IntersectRay(Ray, tL);
        const bool bR =
            (Node.Right != INDEX_NONE) && Nodes[Node.Right].Bounds.IntersectRay(Ray, tR);

        // 가까운 노드를 나중에 push (Front-to-Back), IntersectRay는 push 전 1회만 호출 — 중복 호출
        // 없음
        if (bL && bR)
        {
            if (tL < tR)
            {
                Stack.push_back({Node.Right, tR});
                Stack.push_back({Node.Left, tL});
            }
            else
            {
                Stack.push_back({Node.Left, tL});
                Stack.push_back({Node.Right, tR});
            }
        }
        else if (bL)
            Stack.push_back({Node.Left, tL});
        else if (bR)
            Stack.push_back({Node.Right, tR});
    }
}

bool FBVH::RayQueryTriangleClosest(const FRay& Ray, float& OutT, int32& OutTriangleIndex,
	const std::function<bool(int32, float&)>& IntersectFn) const
{
    if (RootNodeIndex == INDEX_NONE)
        return false;

    struct FEntry
    {
        int32 NodeIndex;
        float tEnter; // AABB 진입 거리 (우선순위 정렬용)
    };

    TArray<FEntry> Stack;
    Stack.reserve(64);

    float tMax = FLT_MAX;
    OutTriangleIndex = -1;

    // Root AABB 충돌 검사
    float tRoot = 0.0f;
    if (!Nodes[RootNodeIndex].Bounds.IntersectRay(Ray, tRoot))
        return false;
    Stack.push_back({RootNodeIndex, tRoot});

    while (!Stack.empty())
    {
        FEntry Entry = Stack.back();
        Stack.pop_back();

		// 이미 찾은 히트보다 먼 AABB는 스킵 (Front-to-Back Early Out)
		if (Entry.tEnter >= tMax)
			continue;

        const FNode& Node = Nodes[Entry.NodeIndex];

        if (Node.IsLeaf())
        {
            for (int32 i = 0; i < Node.ObjectCount; i++)
            {
                const int32 TriangeIndex = ObjectIndices[Node.FirstObject + i];
            }
            continue;
        }

        // 내부 노드: 두 자식 AABB와 교차 거리를 계산해, 가까운 쪽이 나중에 pop되도록 한다.
        float tLeft = FLT_MAX, tRight = FLT_MAX;
        bool  bHitLeft =
            (Node.Left != INDEX_NONE) && Nodes[Node.Left].Bounds.IntersectRay(Ray, tLeft);
        bool bHitRight =
            (Node.Right != INDEX_NONE) && Nodes[Node.Right].Bounds.IntersectRay(Ray, tRight);

        if (bHitLeft && tLeft >= tMax)
            bHitLeft = false;
        if (bHitRight && tRight >= tMax)
            bHitRight = false;

        // 거리가 먼 순서대로 push하면 Stack에서는 먼저 pop되게 된다.
        if (bHitLeft && bHitRight)
        {
            if (tLeft < tRight)
            {
                Stack.push_back({Node.Right, tRight});
                Stack.push_back({Node.Left, tLeft});
            }
            else
            {
                Stack.push_back({Node.Left, tLeft});
                Stack.push_back({Node.Right, tRight});
            }
        }
        else if (bHitLeft)
            Stack.push_back({Node.Left, tLeft});
        else if (bHitRight)
            Stack.push_back({Node.Right, tRight});
    }

    OutT = tMax;
    return OutTriangleIndex != -1;
}

/**
 * @brief Recursively build a BVH node for the given object range.
 * @param ObjectBounds Array of per-object AABBs.
 * @param Start Start index into ObjectIndices.
 * @param Count Number of objects in the range.
 * @return Index of the newly created node.
 */
int32 FBVH::BuildNode(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count)
{
    assert(Count > 0);

    const int32 NodeIndex{static_cast<int32>(Nodes.size())};
    Nodes.emplace_back();

    Nodes[NodeIndex].Bounds = ComputeBounds(ObjectBounds, ObjectIndices, Start, Count);

    // leaf면 여기서 끝
    if (Count <= LeafSize)
    {
        Nodes[NodeIndex].FirstObject = Start;
        Nodes[NodeIndex].ObjectCount = Count;
        return NodeIndex;
    }

    // SAH split
    const FSplitCriterion SplitCriterion{FindSplitPosition(ObjectBounds, Start, Count)};
    const int32           MidIndex{PartitionObjects(ObjectBounds, Start, Count, SplitCriterion)};

    const int32 LeftCount{MidIndex - Start};
    const int32 RightCount{Count - LeftCount};

    // 안전장치: 퇴화 분할 방지
    if (LeftCount <= 0 || RightCount <= 0)
    {
        Nodes[NodeIndex].FirstObject = Start;
        Nodes[NodeIndex].ObjectCount = Count;
        return NodeIndex;
    }

    Nodes[NodeIndex].Left = BuildNode(ObjectBounds, Start, LeftCount);
    Nodes[NodeIndex].Right = BuildNode(ObjectBounds, MidIndex, RightCount);

    // [수정됨] 내부 노드(Internal Node)도 자신이 포함하는 전체 오브젝트 범위를 저장합니다.
    // 자식 노드로 분할될 때 ObjectIndices가 연속적으로 배치되므로 이 범위가 하위 트리의 모든
    // 오브젝트를 포함합니다.
    Nodes[NodeIndex].FirstObject = Start;
    Nodes[NodeIndex].ObjectCount = Count;

    return NodeIndex;
}

/**
 * @brief Compute the AABB that bounds a range of objects.
 * @param ObjectBounds Array of per-object AABBs.
 * @param ObjectIndices Indirection array into ObjectBounds.
 * @param Start Start index into ObjectIndices.
 * @param Count Number of objects in the range.
 * @return Bounding box for the range.
 */
FAABB FBVH::ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& ObjectIndices,
                          int32 Start, int32 Count)
{
    assert(Count > 0);

    FAABB Result = ObjectBounds[ObjectIndices[Start]];

    for (int32 i = 1; i < Count; ++i)
    {
        Result.ExpandToInclude(ObjectBounds[ObjectIndices[Start + i]]);
    }
    return Result;
}

FSplitCriterion FBVH::FindSplitPosition(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count)
{
    assert(Count > 0);

    // SAH (Surface Area Heuristic) using a full sort per axis.
    struct FAxisEntry
    {
        int32 Index;
        float Center;
    };

    auto ComputeSurfaceArea = [](const FAABB& Box)
    {
        const FVector Extent = Box.Max - Box.Min;
        const float   Dx = Extent.X;
        const float   Dy = Extent.Y;
        const float   Dz = Extent.Z;
        return 2.0f * (Dx * Dy + Dy * Dz + Dz * Dx);
    };

    FSplitCriterion Best{};
    float           BestCost = std::numeric_limits<float>::infinity();

    const FAABB NodeBounds = ComputeBounds(ObjectBounds, ObjectIndices, Start, Count);
    const float ParentArea = ComputeSurfaceArea(NodeBounds);
    if (ParentArea <= MathUtil::Epsilon)
    {
        return FindSplitPositionFromBounds(NodeBounds);
    }

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        TArray<FAxisEntry> Entries;
        Entries.reserve(Count);
        for (int32 i = 0; i < Count; ++i)
        {
            const int32   ObjIndex = ObjectIndices[Start + i];
            const FVector Center = ObjectBounds[ObjIndex].GetCenter();
            Entries.push_back({ObjIndex, Center[Axis]});
        }

        std::sort(Entries.begin(), Entries.end(),
                  [](const FAxisEntry& A, const FAxisEntry& B) { return A.Center < B.Center; });

        TArray<FAABB> Prefix;
        TArray<FAABB> Suffix;
        Prefix.resize(Count);
        Suffix.resize(Count);

        Prefix[0] = ObjectBounds[Entries[0].Index];
        for (int32 i = 1; i < Count; ++i)
        {
            Prefix[i] = Prefix[i - 1];
            Prefix[i].ExpandToInclude(ObjectBounds[Entries[i].Index]);
        }

        Suffix[Count - 1] = ObjectBounds[Entries[Count - 1].Index];
        for (int32 i = Count - 2; i >= 0; --i)
        {
            Suffix[i] = Suffix[i + 1];
            Suffix[i].ExpandToInclude(ObjectBounds[Entries[i].Index]);
        }

        for (int32 i = 0; i < Count - 1; ++i)
        {
            const int32 LeftCount = i + 1;
            const int32 RightCount = Count - LeftCount;

            const float LeftArea = ComputeSurfaceArea(Prefix[i]);
            const float RightArea = ComputeSurfaceArea(Suffix[i + 1]);

            const float Cost = (LeftArea / ParentArea) * static_cast<float>(LeftCount) +
                               (RightArea / ParentArea) * static_cast<float>(RightCount);

            if (Cost < BestCost)
            {
                BestCost = Cost;
                Best.Axis = static_cast<EBVHAxis>(Axis);

                const float CenterA = Entries[i].Center;
                const float CenterB = Entries[i + 1].Center;
                Best.Position = 0.5f * (CenterA + CenterB);
            }
        }
    }

    if (!std::isfinite(BestCost))
    {
        return FindSplitPositionFromBounds(NodeBounds);
    }

    return Best;
}

/**
 * @brief Choose a split axis and position from the node bounds.
 * @param NodeAABB Bounds of the current node.
 * @return Split axis and split position (midpoint on the longest axis).
 */
FSplitCriterion FBVH::FindSplitPositionFromBounds(const FAABB& NodeAABB)
{
    // Longest Axis Median Split
    const FVector   Extent{NodeAABB.Max - NodeAABB.Min};
    FSplitCriterion SplitCriterion{};

    if (Extent.X >= Extent.Y && Extent.X >= Extent.Z)
    {
        SplitCriterion.Axis = EBVHAxis::X;
        SplitCriterion.Position = (NodeAABB.Min.X + NodeAABB.Max.X) * 0.5f;
    }
    else if (Extent.Y >= Extent.Z)
    {
        SplitCriterion.Axis = EBVHAxis::Y;
        SplitCriterion.Position = (NodeAABB.Min.Y + NodeAABB.Max.Y) * 0.5f;
    }
    else
    {
        SplitCriterion.Axis = EBVHAxis::Z;
        SplitCriterion.Position = (NodeAABB.Min.Z + NodeAABB.Max.Z) * 0.5f;
    }
    return SplitCriterion;
}

/**
 * @brief Partition objects around the split position (in-place).
 * @param ObjectBounds Array of per-object AABBs.
 * @param Start Start index into ObjectIndices.
 * @param Count Number of objects in the range.
 * @param Criterion Split axis and position.
 * @return Index separating left and right partitions.
 */
int32 FBVH::PartitionObjects(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count,
                             const FSplitCriterion& Criterion)
{
    assert(Count > 0);

    int32 Axis{GetAxisValue(Criterion.Axis)};
    int32 Left{Start};
    int32 Right{Start + Count - 1};

    while (Left <= Right)
    {
        const FAABB&  LeftBox{ObjectBounds[ObjectIndices[Left]]};
        const FVector LeftCenter{LeftBox.GetCenter()};
        const float   LeftValue{LeftCenter[Axis]};

        if (LeftValue < Criterion.Position)
        {
            ++Left;
            continue;
        }

        const FAABB&  RightBox{ObjectBounds[ObjectIndices[Right]]};
        const FVector RightCenter{RightBox.GetCenter()};
        const float   RightValue{RightCenter[Axis]};

        if (RightValue >= Criterion.Position)
        {
            --Right;
            continue;
        }

        std::swap(ObjectIndices[Left], ObjectIndices[Right]);
        ++Left;
        --Right;
    }

    const int32 Mid{Left};

    // 한쪽으로 다 몰리는 퇴화 분할 방지
    if (Mid == Start || Mid == Start + Count)
    {
        const int32 ForcedMid{Start + Count / 2};

        std::sort(ObjectIndices.begin() + Start, ObjectIndices.begin() + Start + Count,
                  [&](int32 A, int32 B)
                  {
                      const FVector CenterA{ObjectBounds[A].GetCenter()};
                      const FVector CenterB{ObjectBounds[B].GetCenter()};
                      return CenterA[Axis] < CenterB[Axis];
                  });
        return ForcedMid;
    }
    return Mid;
}
