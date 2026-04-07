#pragma once

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Core/CoreTypes.h"
#include <functional>

namespace
{
    constexpr int32 INDEX_NONE{-1};

    enum class EBVHAxis
    {
        X,
        Y,
        Z
    };

    // Vector stores X, Y, Z using union to use it like array.
    int32 GetAxisValue(EBVHAxis Axis)
    {
        switch (Axis)
        {
        case EBVHAxis::X:
            return 0;
        case EBVHAxis::Y:
            return 1;
        case EBVHAxis::Z:
            return 2;
        default:
            assert(false && "Invalid EBVHAxis value.");
            return 0;
        }
    }

    struct FSplitCriterion
    {
        EBVHAxis Axis;
        float    Position;
    };
} // namespace

class FBVH
{
  public:
    struct FNode
    {
        FAABB Bounds;

        int32 Left{INDEX_NONE};
        int32 Right{INDEX_NONE};

        int32 FirstObject{INDEX_NONE};
        int32 ObjectCount{0};

        bool IsLeaf() const { return Left == INDEX_NONE && Right == INDEX_NONE; }
    };

  public:
    FBVH() = default;
    ~FBVH() = default;

    /**
     * @brief Build a BVH over the provided object bounds.
     * @param ObjectBounds Array of per-object AABBs.
     * @param InLeafSize Maximum number of objects per leaf node.
     */
    void BuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);
	void BuildBLAS(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);

    /**
     * @brief Refit an existing BVH with updated object bounds.
     * @param ObjectBounds Array of per-object AABBs.
     * @param InLeafSize Maximum number of objects per leaf node.
     */
    void RefitBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize);

    void ReBuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);

    void FrustumQuery(const FFrustum& Frustum, TArray<uint32>& OutIndices,
                       bool bInsideOnly = false) const;
    void RayQuery(const TArray<FAABB>& ObjectBounds, const FRay& Ray,
                  TArray<int32>& OutIndices, TArray<float>& OutTs) const;

	void RayQueryTriangle(const FRay& Ray, TArray<int32>& OutIndices) const;

	// IntersectFn(TriangleIndex, OutHitT) → true이면 히트, OutHitT에 교차 거리를 기록
	bool RayQueryTriangleClosest(const FRay& Ray, float& OutT, int32& OutTriangleIndex,
		const std::function<bool(int32, float&)>& IntersectFn) const;

    void Reset()
    {
        Nodes.clear();
        ObjectIndices.clear();
        RootNodeIndex = INDEX_NONE;
        LeafSize = 0;
    }

    const TArray<FNode>& GetNodes() const { return Nodes; }
    const TArray<int32>& GetObjectIndices() const { return ObjectIndices; }
	const int32 GetRootNodeIndex() const { return RootNodeIndex; }

  private:
    TArray<FNode> Nodes;
    TArray<int32> ObjectIndices;
    int32         RootNodeIndex{INDEX_NONE};
    int32         LeafSize{0};

    // TArray<FAABB>;

  private:
    int32 BuildNode(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count);
    FAABB ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& ObjectIndices,
                        int32 Start, int32 Count);
    FSplitCriterion FindSplitPosition(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count);
    FSplitCriterion FindSplitPositionFromBounds(const FAABB& Bounds);
    int32           PartitionObjects(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count,
                                     const FSplitCriterion& Criterion);
};
