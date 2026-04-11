#pragma once
/**
 * @file BVH.h
 * @brief Bounding Volume Hierarchy (BVH) structure and query interface.
 */

#include "Engine/Core/CoreMinimal.h"
#include "Engine/Core/CoreTypes.h"
#include <cassert>

/** @brief Internal helper symbols used by `FBVH` implementation. */
namespace BVHDetail
{
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
    inline int32 GetAxisValue(EBVHAxis Axis)
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
} // namespace BVHDetail

/**
 * @brief Dynamic Bounding Volume Hierarchy over object AABBs.
 *
 * Each leaf stores exactly one object index. The BVH also maintains an
 * object-to-leaf mapping table so that incremental refit, insertion, removal,
 * and update paths can jump directly to the owning leaf without traversing the
 * tree. Several temporary buffers are kept as members and reused across calls
 * to reduce transient allocations.
 */
class FBVH
{
  public:
    /** @brief Sentinel index used for invalid node/object references. */
    static constexpr int32 INDEX_NONE{-1};

    /** @brief A single BVH node. */
    struct FNode
    {
        FAABB Bounds; /**< Node bounding box. */

        int32 Parent{INDEX_NONE};      /**< Parent node index; `INDEX_NONE` for root. */
        int32 Left{INDEX_NONE};        /**< Left child node index; `INDEX_NONE` for leaf. */
        int32 Right{INDEX_NONE};       /**< Right child node index; `INDEX_NONE` for leaf. */
        int32 ObjectIndex{INDEX_NONE}; /**< Object index stored by leaf nodes (`INDEX_NONE` for internal). */
        int32 Depth{-1};               /**< Depth in BVH tree (`0` for root). */

        /**
         * @brief Check whether this node has no children.
         * @note Released free-list slots also satisfy this predicate because
         * they are reset to `INDEX_NONE` child links.
         */
        bool IsLeaf() const { return Left == INDEX_NONE && Right == INDEX_NONE; }
    };

  public:
    FBVH() = default;
    ~FBVH() = default;

    // Build / Rebuild -------------------------------------------------------

    /**
     * @brief Build a BVH over the provided object bounds.
     * @param ObjectBounds Array of per-object AABBs.
     * @note This resets the current hierarchy and rebuilds both topology and
     * object-to-leaf mappings from scratch.
     */
    void BuildBVH(const TArray<FAABB>& ObjectBounds);

    /**
     * @brief Build a BVH over a subset of objects while preserving their original object indices.
     * @param ObjectBounds Array of per-object AABBs indexed by the final object ids.
     * @param BuildObjectIndices Object indices to include in the new BVH.
     * @note This is useful when the owner keeps sparse object slots but wants a
     * full top-down rebuild over only the currently active objects.
     */
    void BuildBVH(const TArray<FAABB>& ObjectBounds, const TArray<int32>& BuildObjectIndices);

    /**
     * @brief Rebuild BVH from scratch.
     * @param ObjectBounds Array of per-object AABBs.
     */
    void ReBuildBVH(const TArray<FAABB>& ObjectBounds);

    // Refit -----------------------------------------------------------------

    /**
     * @brief Refit only the leaf nodes affected by changed objects and their ancestors.
     * @param ObjectBounds Array of updated per-object AABBs.
     * @param DirtyObjectIndices Indices of objects whose AABBs have changed.
     * @note Dirty objects that are not currently mapped into the BVH are skipped.
     */
    void RefitBVH(const TArray<FAABB>& ObjectBounds, const TArray<int32>& DirtyObjectIndices);

    /**
     * @brief Refit all node bounds in an existing BVH.
     * @param ObjectBounds Array of updated per-object AABBs.
     * @note Only nodes reachable from `RootNodeIndex` are treated as active BVH nodes.
     * @note Reachable nodes are collected in preorder and then processed in reverse
     * order so that children are refit before their parents.
     */
    void RefitBVHFull(const TArray<FAABB>& ObjectBounds);

    // Rotation --------------------------------------------------------------

    /**
     * @brief Try local tree rotations to improve BVH quality.
     * @param ObjectBounds Array of per-object AABBs.
     * @note Only nodes reachable from `RootNodeIndex` participate in optimization.
     * @note The routine performs one full refit first, then applies several
     * bottom-up local optimization passes.
     */
    void RotationBVH(const TArray<FAABB>& ObjectBounds);

    // Dynamic Object Update -------------------------------------------------

    /**
     * @brief Insert a new object AABB into the BVH.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object to insert.
     * @return Index of the inserted leaf node, or `INDEX_NONE` if the index is invalid or already present.
     */
    int32 InsertObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    /**
     * @brief Remove an existing object from the BVH.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object to remove.
     * @return `true` if the object was removed successfully.
     */
    bool RemoveObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    /**
     * @brief Update the BVH after an object's AABB has changed.
     * @param ObjectBounds Array containing the AABBs of all objects.
     * @param ObjectIndex Index of the object whose AABB was updated.
     * @return `true` if the object exists in the BVH and the affected path was updated successfully.
     */
    bool UpdateObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);

    // Query Scratch --------------------------------------------------------

    /**
     * @brief Caller-owned scratch buffers for frustum traversal.
     * @note Reuse one scratch instance per worker/thread to avoid allocations
     * while keeping concurrent queries on the same `FBVH` instance thread-safe.
     */
    struct FFrustumQueryScratch
    {
        struct FStackEntry
        {
            int32 NodeIndex{INDEX_NONE};
            bool  bAssumeInside{false};
        };

        TArray<FStackEntry> TraversalStack;
    };

    /**
     * @brief Caller-owned scratch buffers for ray traversal, pruning, and hit sorting.
     * @note Reuse one scratch instance per worker/thread to avoid allocations
     * while keeping concurrent queries on the same `FBVH` instance thread-safe.
     */
    struct FRayQueryScratch
    {
        struct FStackEntry
        {
            int32 NodeIndex{INDEX_NONE};
            float TEnter{0.0f};
        };

        TArray<FStackEntry> NodeStack;
        TArray<int32> Order;
        TArray<int32> SortedIndices;
        TArray<float> SortedTs;
    };

    // Queries ---------------------------------------------------------------

    /**
     * @brief Collect object indices that overlap the input frustum.
     * @param Frustum Query frustum.
     * @param OutIndices Output object indices.
     * @param Scratch Caller-owned traversal scratch. Use a different instance for each concurrent query.
     * @param bInsideOnly If `true`, return only objects fully inside the frustum.
     */
    void FrustumQuery(const FFrustum& Frustum, TArray<int32>& OutIndices, FFrustumQueryScratch& Scratch,
                      bool bInsideOnly = false) const;

    /**
     * @brief Return the closest leaf-AABB hit along the ray.
     * @param Ray Query ray.
     * @param OutObjectIndex Closest intersected object index, or `INDEX_NONE` on miss.
     * @param OutT Ray distance to the closest leaf AABB hit.
     * @param Scratch Caller-owned traversal scratch. Use a different instance for each concurrent query.
     * @return `true` if any leaf AABB was hit.
     * @note This is a broad-phase result over BVH leaf bounds. It does not guarantee
     * the returned object is also the closest triangle-level hit for the mesh.
     */
    bool RayQueryClosestAABB(const FRay& Ray, int32& OutObjectIndex, float& OutT, FRayQueryScratch& Scratch) const;

    /**
     * @brief Collect object indices intersected by a ray and corresponding hit distances.
     * @param ObjectBounds Array of per-object AABBs.
     * @param Ray Query ray.
     * @param OutIndices Output intersected object indices.
     * @param OutTs Output ray hit distances aligned with `OutIndices`.
     * @param Scratch Caller-owned traversal and sorting scratch. Use a different
     * instance for each concurrent query.
     * @note Results are sorted nearest-first. Small hit sets are reordered in place;
     * larger ones use the scratch buffers to avoid per-call allocations.
     */
    void RayQuery(const TArray<FAABB>& ObjectBounds, const FRay& Ray, TArray<int32>& OutIndices,
                  TArray<float>& OutTs, FRayQueryScratch& Scratch) const;


    // State -----------------------------------------------------------------

    /**
     * @brief Clear all BVH state while keeping allocated capacity for reuse.
     * @note This is optimized for rebuild-heavy workflows. Use a container-level
     * shrink operation outside this class if peak capacity must be released.
     */
    void Reset()
    {
        Nodes.clear();
        FreeNodeIndices.clear();
        ObjectToLeafNode.clear();

        RefitLeafMarks.clear();
        RefitParentMarks.clear();
        RefitDirtyLeafIndices.clear();
        RefitDirtyParentIndices.clear();
        BuildAxisEntriesScratch.clear();
        BuildPrefixBoundsScratch.clear();
        BuildSuffixBoundsScratch.clear();
        ReachableNodeIndicesScratch.clear();
        TraversalStackScratch.clear();
        TraversalVisitMarks.clear();
        PathToRootScratch.clear();
        RefitMark = 1;
        TraversalVisitMark = 1;

        RootNodeIndex = INDEX_NONE;
    }

    /**
     * @brief Get raw node storage, including reusable free slots after dynamic updates.
     * @note Only nodes reachable from `RootNodeIndex` represent the active tree.
     */
    const TArray<FNode>& GetNodes() const { return Nodes; }

    /** @brief Get mapping from object index to containing leaf node index. */
    const TArray<int32>& GetObjectToLeafNode() const { return ObjectToLeafNode; }

    /** @brief Get root node index, or `INDEX_NONE` if tree is empty. */
    int32 GetRootNodeIndex() const { return RootNodeIndex; }

  private:
    // Core Storage ----------------------------------------------------------
    TArray<FNode> Nodes; /**< Storage slots for active BVH nodes and reusable free slots; released nodes stay here so
                            indices remain stable. */
    TArray<int32> FreeNodeIndices;  /**< Indices of released node slots that can be reused by later insertions. */
    TArray<int32> ObjectToLeafNode; /**< Mapping from object index to its containing leaf node index, maintained only by
                                       structural build/insert/remove paths. */

    int32 RootNodeIndex{INDEX_NONE}; /**< Root node index; `INDEX_NONE` if empty. */

    // Build Helpers ---------------------------------------------------------

    /** @brief Recursively build a node for object range `[Start, Start + Count)` in the build scratch index array. */
    int32 BuildNode(const TArray<FAABB>& ObjectBounds, TArray<int32>& BuildObjectIndices, int32 Start, int32 Count,
                    int32 Depth);

    /** @brief Compute bounds over object range `[Start, Start + Count)` in the supplied build index array. */
    FAABB ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& InObjectIndices, int32 Start,
                        int32 Count);

    // Split Helpers ---------------------------------------------------------
    /** @brief Split axis and split position used to partition objects. */
    struct FSplitCriterion
    {
        BVHDetail::EBVHAxis Axis{BVHDetail::EBVHAxis::X};
        float               Position{0.0f};
    };
    /** @brief Temporary entry used while sorting object centers along a candidate axis. */
    struct FBuildAxisEntry
    {
        int32 Index{INDEX_NONE};
        float Center{0.0f};
    };
    /** @brief Find split axis/position for object range `[Start, Start + Count)` in the build scratch index array. */
    FSplitCriterion FindSplitPosition(const TArray<FAABB>& ObjectBounds, const TArray<int32>& BuildObjectIndices,
                                      int32 Start, int32 Count);

    /** @brief Fallback split choice from node bounds (longest-axis median). */
    FSplitCriterion FindSplitPositionFromBounds(const FAABB& Bounds);

    /** @brief Partition object range `[Start, Start + Count)` in-place inside the build scratch index array. */
    int32 PartitionObjects(const TArray<FAABB>& ObjectBounds, TArray<int32>& BuildObjectIndices, int32 Start,
                           int32 Count, const FSplitCriterion& Criterion);

    // Refit Helpers ---------------------------------------------------------

    TArray<uint32> RefitLeafMarks;          /**< Generation marks used to deduplicate dirty leaves. */
    TArray<uint32> RefitParentMarks;        /**< Generation marks used to deduplicate dirty parents. */
    TArray<int32>  RefitDirtyLeafIndices;   /**< Unique dirty leaves collected for one incremental refit call. */
    TArray<int32>  RefitDirtyParentIndices; /**< Unique dirty parents collected for one incremental refit call. */
    uint32         RefitMark{1};            /**< Current generation value for the incremental refit scratch marks. */

    TArray<FBuildAxisEntry> BuildAxisEntriesScratch;  /**< Reused per-axis center list for SAH candidate evaluation. */
    TArray<FAABB>           BuildPrefixBoundsScratch; /**< Reused prefix bounds scratch for SAH evaluation. */
    TArray<FAABB>           BuildSuffixBoundsScratch; /**< Reused suffix bounds scratch for SAH evaluation. */

    mutable TArray<int32>  ReachableNodeIndicesScratch; /**< Reused list of active nodes collected from the root. */
    mutable TArray<int32>  TraversalStackScratch;       /**< Reused DFS stack for active-tree traversals. */
    mutable TArray<uint32> TraversalVisitMarks;         /**< Debug-only visit marks for validating acyclic traversal. */
    mutable uint32         TraversalVisitMark{1};       /**< Debug-only generation value for traversal marks. */

    /** @brief Prepare incremental-refit scratch buffers and advance their generation marks. */
    void PrepareRefitScratchBuffers();
    /**
     * @brief Recompute the bounds of one node from either its object or its children.
     * @param ObjectBounds Updated per-object AABBs.
     * @param NodeIndex Node index to refit.
     * @note Released free-list slots and invalid references are ignored defensively.
     */
    void RefitNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);
    /** @brief Refit one node and every ancestor reachable through parent links. */
    void RefitUpwards(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);

    TArray<int32> PathToRootScratch; /**< Reused list of node indices from a local change up to the root. */

    // Rotation Helpers ------------------------------------------------------

    struct FRotationCandidate
    {
        bool bValid{false};
        // `true` when rotating around the left child subtree, otherwise the right child subtree.
        bool bRotateLeftChild{false};
        bool bUseFirstGrandChild{false}; // Selects which grandchild participates in the candidate swap.

        int32 NodeIndex{INDEX_NONE};

        float OldCost{0.0f};
        float NewCost{0.0f};

        float Gain() const { return OldCost - NewCost; }
    };

    /** @brief Recompute depth values for the subtree rooted at `NodeIndex`. */
    void UpdateDepthsFromNode(int32 NodeIndex, int32 Depth);
    /** @brief Evaluate the best beneficial swap between `Node.Right` and one grandchild under `Node.Left`. */
    FRotationCandidate EvaluateRotateWithLeftChild(int32 NodeIndex) const;
    /** @brief Evaluate the best beneficial swap between `Node.Left` and one grandchild under `Node.Right`. */
    FRotationCandidate EvaluateRotateWithRightChild(int32 NodeIndex) const;
    /**
     * @brief Apply one validated local rotation candidate.
     * @param ObjectBounds Updated per-object AABBs.
     * @param Candidate Previously evaluated rotation candidate.
     * @return `true` if the rotation was applied.
     * @note Leaf ownership does not change, so the object-to-leaf mapping remains valid.
     */
    bool ApplyRotation(const TArray<FAABB>& ObjectBounds, const FRotationCandidate& Candidate);
    /** @brief Choose the better rotation candidate for a node and apply it if beneficial. */
    bool TryRotateNodeBest(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);

    // Dynamic Object Update Helpers -----------------------------------------

    /** @brief Allocate a new node slot and return its index. */
    int32 AllocateNode();
    /** @brief Return a node slot to the free list after it becomes unreachable. */
    void ReleaseNode(int32 NodeIndex);
    /** @brief Ensure the object-to-leaf mapping table can store the given object index. */
    void EnsureObjectMappingCapacity(int32 ObjectIndex);
    /** @brief Record that a leaf node now owns the given object index. */
    void LinkObjectToLeaf(int32 ObjectIndex, int32 LeafNodeIndex);
    /** @brief Clear the mapping for an object if it still points at the expected leaf. */
    void UnlinkObjectFromLeaf(int32 ObjectIndex, int32 ExpectedLeafNodeIndex);
    /** @brief Create a single-object leaf node for dynamic insertion. */
    int32 CreateLeafNode(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);
    /** @brief Look up the leaf linked to the given object index through the validated mapping table without traversing
     * the tree. */
    int32 FindLeafNodeIndexByObject(int32 ObjectIndex) const;
    /** @brief Descend the tree and find the best sibling leaf/subtree for the given bounds. */
    int32 FindBestSibling(const FAABB& NewBounds) const;
    /** @brief Insert a new leaf into the BVH under the best sibling. */
    int32 InsertLeafNode(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex);
    /** @brief Remove a leaf node from the BVH and promote its sibling. */
    void RemoveLeafNode(const TArray<FAABB>& ObjectBounds, int32 LeafNodeIndex);
    /** @brief Refit bounds from the given node to the root after a topology change. */
    void RefitUpwardsAfterStructuralChange(const TArray<FAABB>& ObjectBounds, int32 NodeIndex);
    /**
     * @brief Apply local rotations along the path from a changed node to the root.
     * @note The path is collected first so rotations do not invalidate the traversal order.
     */
    void OptimizeAlongPath(const TArray<FAABB>& ObjectBounds, int32 StartNodeIndex);

    // Shared Helpers / Validation ------------------------------------------

    /** @brief Compute surface area of an AABB. */
    static float ComputeSurfaceArea(const FAABB& Box);

    /** @brief Return union bounds of two AABBs. */
    static FAABB UnionBounds(const FAABB& A, const FAABB& B);
    /**
     * @brief Gather active BVH nodes reachable from the root in preorder.
     * @param OutNodeIndices Output storage filled with active node indices only.
     * @note The release path assumes a valid tree topology. Debug builds additionally
     * track visited nodes to catch duplicate visits caused by corrupted links.
     */
    void CollectReachableNodes(TArray<int32>& OutNodeIndices) const;

    /** @brief Validate BVH topology consistency with debug-only assertions. */
    void ValidateBVH() const;
};
