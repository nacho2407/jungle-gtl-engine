#pragma once
/**
 * @file BVH.h
 * @brief Bounding Volume Hierarchy (BVH) structure and query interface.
 */

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Core/CoreTypes.h"
#include <cassert>
#include <functional>

namespace
{
    /** @brief Sentinel index used for invalid node/object references. */
    constexpr int32 INDEX_NONE{-1};

    /** @brief Axis selector for split-plane computation. */
    enum class EBVHAxis
    {
        X,
        Y,
        Z
    };

    /**
     * @brief Convert an axis enum to `FVector` component index.
     * @param Axis Axis enum value.
     * @return `0` for X, `1` for Y, `2` for Z.
     */
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

    /** @brief Split axis and split position used to partition objects. */
    struct FSplitCriterion
    {
        EBVHAxis Axis;
        float    Position;
    };
} // namespace

/**
 * @brief Bounding Volume Hierarchy over object AABBs.
 *
 * The BVH stores an index indirection table for input objects and supports
 * build/refit/rebuild, local rotation optimization, and frustum/ray queries.
 */
class FBVH
{
  public:
    /** @brief A single BVH node. */
    struct FNode
    {
        FAABB Bounds; /**< Node bounding box. */

        int32 Parent{INDEX_NONE}; /**< Parent node index; `INDEX_NONE` for root. */
        int32 Left{INDEX_NONE};   /**< Left child node index; `INDEX_NONE` for leaf. */
        int32 Right{INDEX_NONE};  /**< Right child node index; `INDEX_NONE` for leaf. */

        int32 FirstObject{INDEX_NONE}; /**< Start offset in `ObjectIndices` covered by this node. */
        int32 ObjectCount{0};          /**< Number of objects covered by this node. */

        /** @brief Check whether this node has no children. */
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

    /**
     * @brief Build a BLAS using the same implementation as `BuildBVH`.
     * @param ObjectBounds Array of per-object AABBs.
     * @param InLeafSize Maximum number of objects per leaf node.
     */
    void BuildBLAS(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);

    /**
     * @brief Refit an existing BVH with updated object bounds.
     * @param ObjectBounds Array of per-object AABBs.
     */
    void RefitBVH(const TArray<FAABB>& ObjectBounds);

    /**
     * @brief Try local tree rotations to improve BVH quality, then refit bounds.
     * @param ObjectBounds Array of per-object AABBs.
     */
    void RotationBVH(const TArray<FAABB>& ObjectBounds);

    /**
     * @brief Rebuild BVH from scratch using provided or previous leaf size.
     * @param ObjectBounds Array of per-object AABBs.
     * @param InLeafSize Maximum number of objects per leaf node.
     * @note If `InLeafSize <= 0`, existing/default leaf size is used.
     */
    void ReBuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);

    /**
     * @brief Collect object indices that overlap the input frustum.
     * @param Frustum Query frustum.
     * @param OutIndices Output object indices.
     * @param bInsideOnly If `true`, return only objects fully inside the frustum.
     */
    void FrustumQuery(const FFrustum& Frustum, TArray<uint32>& OutIndices,
                      bool bInsideOnly = false) const;

    /**
     * @brief Collect object indices intersected by a ray and corresponding hit distances.
     * @param ObjectBounds Array of per-object AABBs.
     * @param Ray Query ray.
     * @param OutIndices Output intersected object indices.
     * @param OutTs Output ray hit distances aligned with `OutIndices`.
     */
    void RayQuery(const TArray<FAABB>& ObjectBounds, const FRay& Ray, TArray<int32>& OutIndices,
                  TArray<float>& OutTs) const;

    /** @brief Clear all BVH nodes and reset state. */
    void Reset()
    {
        Nodes.clear();
        ObjectIndices.clear();
        RootNodeIndex = INDEX_NONE;
        LeafSize = 0;
    }

    /** @brief Get all BVH nodes in storage order. */
    const TArray<FNode>& GetNodes() const { return Nodes; }
    /** @brief Get object indirection table (`node range -> object index`). */
    const TArray<int32>& GetObjectIndices() const { return ObjectIndices; }
    /** @brief Get root node index, or `INDEX_NONE` if tree is empty. */
    const int32 GetRootNodeIndex() const { return RootNodeIndex; }

  private:
    TArray<FNode> Nodes;         /**< BVH nodes in contiguous storage. */
    TArray<int32> ObjectIndices; /**< Object index indirection table used during partitioning. */
    int32         RootNodeIndex{INDEX_NONE}; /**< Root node index; `INDEX_NONE` if empty. */
    int32         LeafSize{0};               /**< Maximum objects per leaf during build. */

  private:
    /** @brief Recursively build a node for object range `[Start, Start + Count)`. */
    int32 BuildNode(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count);
    /** @brief Compute bounds over object range `[Start, Start + Count)`. */
    FAABB ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& ObjectIndices,
                        int32 Start, int32 Count);
    /** @brief Find split axis/position for object range `[Start, Start + Count)`. */
    FSplitCriterion FindSplitPosition(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count);
    /** @brief Fallback split choice from node bounds (longest-axis median). */
    FSplitCriterion FindSplitPositionFromBounds(const FAABB& Bounds);
    /** @brief Partition object range `[Start, Start + Count)` in-place around split plane. */
    int32 PartitionObjects(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count,
                           const FSplitCriterion& Criterion);

  private:
    /** @brief Compute surface area of an AABB. */
    static float ComputeSurfaceArea(const FAABB& Box);
    /** @brief Return union bounds of two AABBs. */
    static FAABB UnionBounds(const FAABB& A, const FAABB& B);

    /** @brief Recompute bounds of a node from children or covered objects. */
    void RefitNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);
    /** @brief Rebuild parent indices from child links. */
    void RebuildParentLinks();

    /** @brief Try both local rotation patterns for one node. */
    bool TryRotateNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);
    /** @brief Try rotations where the left child is expanded/rearranged. */
    bool TryRotateWithLeftChild(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);
    /** @brief Try rotations where the right child is expanded/rearranged. */
    bool TryRotateWithRightChild(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);

    /** @brief Validate BVH topology consistency (debug assertions). */
    void ValidateBVH() const;
};
