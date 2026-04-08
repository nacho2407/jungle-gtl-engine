#pragma once
/**
 * @file BVH.h
 * @brief Bounding Volume Hierarchy (BVH) structure and query interface.
 */

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Core/CoreTypes.h"

namespace
{
    /** @brief Sentinel index used for invalid node/object references. */
    static constexpr int32 INDEX_NONE{-1};
    static constexpr float kBVHValidateEpsilon{1e-5f};

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

        int32 Depth{-1};

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

    //-------------------------------------------------------------------------------------------------
  private:
    TArray<uint32> RefitLeafMarks;
    TArray<uint32> RefitParentMarks;
    TArray<int32>  RefitDirtyLeafIndices;
    TArray<int32>  RefitDirtyParentIndices;
    uint32         RefitMark{1};

    void PrepareRefitScratchBuffers();

  public:
    /**
     * @brief Refit only the leaf nodes affected by changed objects and their ancestor nodes.
     * @param ObjectBounds Array of updated per-object AABBs.
     * @param DirtyObjectIndices Indices of objects whose AABBs have changed.
     */
    void RefitBVH(const TArray<FAABB>& ObjectBounds, const TArray<int32>& DirtyObjectIndices);
    /**
     * @brief Refit an existing BVH with updated object bounds.
     * @param ObjectBounds Array of per-object AABBs.
     */
    void RefitBVHFull(const TArray<FAABB>& ObjectBounds);
    //-------------------------------------------------------------------------------------------------

    //-------------------------------------------------------------------------------------------------
  private:
    struct FRotationCandidate
    {
        bool bValid{false};

        // 어떤 패턴인지
        bool bRotateLeftChild{false};    // true면 left-child 기반, false면 right-child 기반
        bool bUseFirstGrandChild{false}; // left면 A.Left / right면 B.Left 사용 여부

        int32 NodeIndex{INDEX_NONE};

        float OldCost{0.0f};
        float NewCost{0.0f};

        float Gain() const { return OldCost - NewCost; }
    };
    void UpdateDepthsFromNode(int32 NodeIndex, int32 Depth);
    void RefitUpwards(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);

    FRotationCandidate EvaluateRotateWithLeftChild(int32 NodeIndex) const;
    FRotationCandidate EvaluateRotateWithRightChild(int32 NodeIndex) const;

    bool ApplyRotation(const TArray<FAABB>& ObjectBounds, const FRotationCandidate& Candidate);
    bool TryRotateNodeBest(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);

  public:
    /**
     * @brief Try local tree rotations to improve BVH quality, then refit bounds.
     * @param ObjectBounds Array of per-object AABBs.
     */
    void RotationBVH(const TArray<FAABB>& ObjectBounds);
    //-------------------------------------------------------------------------------------------------

    /**
     * @brief Rebuild BVH from scratch using provided or previous leaf size.
     * @param ObjectBounds Array of per-object AABBs.
     * @param InLeafSize Maximum number of objects per leaf node.
     * @note If `InLeafSize <= 0`, existing/default leaf size is used.
     */
    void ReBuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize = 4);

    /**
     * @brief Inserts a new object AABB into the BVH.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object to insert.
     * @return Index of the inserted leaf node, or INDEX_NONE on failure.
     */
    int32 InsertObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    /**
     * @brief Removes an existing object from the BVH.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object to remove.
     * @return True if the object was removed successfully, false otherwise.
     */
    bool RemoveObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    /**
     * @brief Updates the BVH after an object's AABB has changed.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object whose AABB was updated.
     * @return True if the BVH was updated successfully, false otherwise.
     */
    bool UpdateObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    /**
     * @brief Collect object indices that overlap the input frustum.
     * @param Frustum Query frustum.
     * @param OutIndices Output object indices.
     * @param bInsideOnly If `true`, return only objects fully inside the frustum.
     */
    void FrustumQuery(const FFrustum& Frustum, TArray<uint32>& OutIndices, bool bInsideOnly = false) const;

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
        ObjectToLeafNode.clear();
        // NodeDepths.clear();

        RefitLeafMarks.clear();
        RefitParentMarks.clear();
        RefitDirtyLeafIndices.clear();
        RefitDirtyParentIndices.clear();
        RefitMark = 1;

        RootNodeIndex = INDEX_NONE;
        LeafSize = 0;
    }

    /** @brief Get all BVH nodes in storage order. */
    const TArray<FNode>& GetNodes() const { return Nodes; }
    /** @brief Get object indirection table (`node range -> object index`). */
    const TArray<int32>& GetObjectIndices() const { return ObjectIndices; }
    /** @brief Get the mapping from object index to the leaf node that currently contains it. */
    const TArray<int32>& GetObjectToLeafNode() const { return ObjectToLeafNode; }
    /** @brief Get root node index, or `INDEX_NONE` if tree is empty. */
    const int32 GetRootNodeIndex() const { return RootNodeIndex; }

  private:
    TArray<FNode> Nodes; /**< BVH nodes in contiguous storage. */
    // TArray<int32> NodeDepths;                /**< Depth of each node from the root (root = 0). */
    TArray<int32> ObjectIndices;             /**< Object index indirection table used during partitioning. */
    TArray<int32> ObjectToLeafNode;          /**< Mapping from object index to its containing leaf node index. */
    int32         RootNodeIndex{INDEX_NONE}; /**< Root node index; `INDEX_NONE` if empty. */
    int32         LeafSize{0};               /**< Maximum objects per leaf during build. */

  private:
    /** @brief Recursively build a node for object range `[Start, Start + Count)`. */
    int32 BuildNode(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count, int32 Depth);
    /** @brief Compute bounds over object range `[Start, Start + Count)`. */
    FAABB ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& ObjectIndices, int32 Start,
                        int32 Count);
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
    /** @brief Validate BVH topology consistency (debug assertions). */
    void ValidateBVH() const;
};
