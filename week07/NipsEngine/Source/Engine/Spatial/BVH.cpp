#include "BVH.h"

#include <algorithm>
#include <cfloat>
#include <limits>

/**
 * @file BVH.cpp
 * @brief FBVH build, refit, optimization, and query implementation.
 */

#if defined(_DEBUG)
#define BVH_VALIDATE() ValidateBVH()
#else
#define BVH_VALIDATE() ((void)0)
#endif

using BVHDetail::EBVHAxis;
using BVHDetail::GetAxisValue;

// ============================================================================
// Build / Rebuild
// ============================================================================

/**
 * @brief Build a BVH over the provided object bounds.
 * @param ObjectBounds Array of per-object AABBs.
 */
void FBVH::BuildBVH(const TArray<FAABB>& ObjectBounds)
{
    const int32 ObjectCount = static_cast<int32>(ObjectBounds.size());
    TArray<int32> BuildObjectIndices;
    BuildObjectIndices.resize(ObjectCount);
    for (int32 i{0}; i < ObjectCount; ++i)
    {
        BuildObjectIndices[i] = i;
    }
    BuildBVH(ObjectBounds, BuildObjectIndices);
}

/**
 * @brief Build a BVH over a subset of objects while preserving their original indices.
 * @param ObjectBounds Array of per-object AABBs.
 * @param BuildObjectIndices Object indices to include in the BVH.
 */
void FBVH::BuildBVH(const TArray<FAABB>& ObjectBounds, const TArray<int32>& BuildObjectIndices)
{
    Reset();

    TArray<int32> FilteredBuildObjectIndices;
    FilteredBuildObjectIndices.reserve(BuildObjectIndices.size());

    int32 MaxObjectIndex = INDEX_NONE;
    for (int32 ObjectIndex : BuildObjectIndices)
    {
        if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectBounds.size()))
        {
            continue;
        }

        FilteredBuildObjectIndices.push_back(ObjectIndex);
        if (ObjectIndex > MaxObjectIndex)
        {
            MaxObjectIndex = ObjectIndex;
        }
    }

    const int32 ObjectCount = static_cast<int32>(FilteredBuildObjectIndices.size());
    if (ObjectCount == 0)
    {
        RootNodeIndex = INDEX_NONE;
        return;
    }

    Nodes.reserve(ObjectCount * 2 - 1);
    ObjectToLeafNode.resize(std::max(static_cast<int32>(ObjectBounds.size()), MaxObjectIndex + 1), INDEX_NONE);
    RootNodeIndex = BuildNode(ObjectBounds, FilteredBuildObjectIndices, 0, ObjectCount, 0);

    BVH_VALIDATE();
}

/**
 * @brief Rebuild BVH from scratch.
 * @param ObjectBounds Per-object AABBs used to rebuild.
 */
void FBVH::ReBuildBVH(const TArray<FAABB>& ObjectBounds)
{
    BuildBVH(ObjectBounds);
}

// ============================================================================
// Refit
// ============================================================================

/**
 * @brief Incrementally refit only the leaves referenced by dirty objects and their ancestors.
 * @param ObjectBounds Updated per-object AABBs.
 * @param DirtyObjectIndices Object indices whose bounds changed since the last update.
 *
 * The object-to-leaf mapping is treated as authoritative here. Objects that are
 * not currently linked into the BVH are ignored instead of being searched for.
 */
void FBVH::RefitBVH(const TArray<FAABB>& ObjectBounds, const TArray<int32>& DirtyObjectIndices)
{
    if (RootNodeIndex == INDEX_NONE || Nodes.empty() || DirtyObjectIndices.empty())
    {
        return;
    }

    PrepareRefitScratchBuffers();

    // Gather unique dirty leaves from the dirty object list.
    for (int32 ObjIndex : DirtyObjectIndices)
    {
        if (ObjIndex < 0 || ObjIndex >= static_cast<int32>(ObjectBounds.size()))
        {
            continue;
        }

        const int32 LeafNodeIndex = FindLeafNodeIndexByObject(ObjIndex);
        if (LeafNodeIndex == INDEX_NONE)
        {
            continue;
        }

        if (RefitLeafMarks[LeafNodeIndex] != RefitMark)
        {
            RefitLeafMarks[LeafNodeIndex] = RefitMark;
            RefitDirtyLeafIndices.push_back(LeafNodeIndex);
        }
    }

    // Refit each dirty leaf, then collect its ancestors without duplicates.
    for (int32 LeafNodeIndex : RefitDirtyLeafIndices)
    {
        RefitNode(ObjectBounds, LeafNodeIndex);

        // Walk toward the root and record each parent once.
        int32 ParentIndex = Nodes[LeafNodeIndex].Parent;
        while (ParentIndex != INDEX_NONE)
        {
            if (RefitParentMarks[ParentIndex] != RefitMark)
            {
                RefitParentMarks[ParentIndex] = RefitMark;
                RefitDirtyParentIndices.push_back(ParentIndex);
            }

            ParentIndex = Nodes[ParentIndex].Parent;
        }
    }

    std::sort(RefitDirtyParentIndices.begin(), RefitDirtyParentIndices.end(),
              [&](int32 A, int32 B)
              {
                  if (Nodes[A].Depth != Nodes[B].Depth)
                  {
                      return Nodes[A].Depth > Nodes[B].Depth;
                  }
                  return A > B;
              });

    // Refit parents deepest-first so child bounds are already up to date.
    for (int32 ParentIndex : RefitDirtyParentIndices)
    {
        RefitNode(ObjectBounds, ParentIndex);
    }

    BVH_VALIDATE();
}

/**
 * @brief Prepare and recycle temporary buffers used by incremental refit.
 */
void FBVH::PrepareRefitScratchBuffers()
{
    const int32 NodeCount = static_cast<int32>(Nodes.size());

    if (static_cast<int32>(RefitLeafMarks.size()) < NodeCount)
    {
        RefitLeafMarks.resize(NodeCount, 0u);
    }
    if (static_cast<int32>(RefitParentMarks.size()) < NodeCount)
    {
        RefitParentMarks.resize(NodeCount, 0u);
    }

    RefitDirtyLeafIndices.clear();
    RefitDirtyParentIndices.clear();

    ++RefitMark;
    if (RefitMark == 0)
    {
        std::fill(RefitLeafMarks.begin(), RefitLeafMarks.end(), 0u);
        std::fill(RefitParentMarks.begin(), RefitParentMarks.end(), 0u);
        RefitMark = 1;
    }
}

/**
 * @brief Gather currently reachable active-tree nodes from the root in preorder.
 * @param OutNodeIndices Output node indices.
 * @note Release builds assume the structure is a valid tree. Debug builds keep
 * a reusable visit-mark array to detect duplicate visits caused by bad links.
 *
 * `Nodes` may contain reusable free-list slots, so callers must not treat raw
 * storage order as the active tree. This helper rebuilds the active traversal
 * order from `RootNodeIndex` each time it is needed.
 */
void FBVH::CollectReachableNodes(TArray<int32>& OutNodeIndices) const
{
    OutNodeIndices.clear();

    if (RootNodeIndex == INDEX_NONE)
    {
        return;
    }

    assert(RootNodeIndex >= 0 && RootNodeIndex < static_cast<int32>(Nodes.size()));

    OutNodeIndices.reserve(Nodes.size());
    TraversalStackScratch.clear();
    TraversalStackScratch.reserve(Nodes.size());

#if defined(_DEBUG)
    if (static_cast<int32>(TraversalVisitMarks.size()) < static_cast<int32>(Nodes.size()))
    {
        TraversalVisitMarks.resize(Nodes.size(), 0u);
    }

    ++TraversalVisitMark;
    if (TraversalVisitMark == 0)
    {
        std::fill(TraversalVisitMarks.begin(), TraversalVisitMarks.end(), 0u);
        TraversalVisitMark = 1;
    }

    TraversalVisitMarks[RootNodeIndex] = TraversalVisitMark;
#endif

    TraversalStackScratch.push_back(RootNodeIndex);

    while (!TraversalStackScratch.empty())
    {
        const int32 NodeIndex = TraversalStackScratch.back();
        TraversalStackScratch.pop_back();

        assert(NodeIndex >= 0 && NodeIndex < static_cast<int32>(Nodes.size()));
        OutNodeIndices.push_back(NodeIndex);

        const FNode& Node = Nodes[NodeIndex];
        if (!Node.IsLeaf())
        {
            if (Node.Right != INDEX_NONE)
            {
                assert(Node.Right >= 0 && Node.Right < static_cast<int32>(Nodes.size()));
                if (Node.Right >= 0 && Node.Right < static_cast<int32>(Nodes.size()))
                {
#if defined(_DEBUG)
                    assert(TraversalVisitMarks[Node.Right] != TraversalVisitMark);
                    TraversalVisitMarks[Node.Right] = TraversalVisitMark;
#endif
                    // Push right first so the LIFO stack visits left first.
                    TraversalStackScratch.push_back(Node.Right);
                }
            }
            if (Node.Left != INDEX_NONE)
            {
                assert(Node.Left >= 0 && Node.Left < static_cast<int32>(Nodes.size()));
                if (Node.Left >= 0 && Node.Left < static_cast<int32>(Nodes.size()))
                {
#if defined(_DEBUG)
                    assert(TraversalVisitMarks[Node.Left] != TraversalVisitMark);
                    TraversalVisitMarks[Node.Left] = TraversalVisitMark;
#endif
                    TraversalStackScratch.push_back(Node.Left);
                }
            }
        }
    }
}

/**
 * @brief Refit all reachable node bounds in an existing BVH.
 * @param ObjectBounds Updated per-object AABBs.
 * @note Nodes are visited in reverse preorder so every child is processed before
 * its parent without needing a separate depth sort.
 */
void FBVH::RefitBVHFull(const TArray<FAABB>& ObjectBounds)
{
    if (RootNodeIndex == INDEX_NONE || Nodes.empty())
    {
        return;
    }

    CollectReachableNodes(ReachableNodeIndicesScratch);

    for (int32 OrderIndex = static_cast<int32>(ReachableNodeIndicesScratch.size()) - 1; OrderIndex >= 0; --OrderIndex)
    {
        RefitNode(ObjectBounds, ReachableNodeIndicesScratch[OrderIndex]);
    }
    BVH_VALIDATE();
}

/**
 * @brief Refit one node's bounds from either its object or its children.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Node index to refit.
 * @note Released slots are reset to look like leaves, so `ObjectIndex` is
 * validated before a raw storage entry is treated as an active leaf.
 */
void FBVH::RefitNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
    assert(NodeIndex >= 0 && NodeIndex < static_cast<int32>(Nodes.size()));
    FNode& Node = Nodes[NodeIndex];

    if (Node.IsLeaf())
    {
        if (Node.ObjectIndex == INDEX_NONE)
        {
            return;
        }

        assert(Node.ObjectIndex >= 0);
        if (Node.ObjectIndex < 0 || Node.ObjectIndex >= static_cast<int32>(ObjectBounds.size()))
        {
            return;
        }

        Node.Bounds = ObjectBounds[Node.ObjectIndex];
        return;
    }

    assert(Node.ObjectIndex == INDEX_NONE);
    Node.ObjectIndex = INDEX_NONE;

    assert(Node.Left != INDEX_NONE && Node.Right != INDEX_NONE);
    if (Node.Left == INDEX_NONE || Node.Right == INDEX_NONE)
    {
        return;
    }

    assert(Node.Left >= 0 && Node.Left < static_cast<int32>(Nodes.size()));
    assert(Node.Right >= 0 && Node.Right < static_cast<int32>(Nodes.size()));
    assert(Node.Left != Node.Right);
    if (Node.Left < 0 || Node.Left >= static_cast<int32>(Nodes.size()) || Node.Right < 0 ||
        Node.Right >= static_cast<int32>(Nodes.size()) || Node.Left == Node.Right)
    {
        return;
    }

    Node.Bounds = UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
}

/**
 * @brief Refit from a node toward the root through parent links.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Starting node index.
 */
void FBVH::RefitUpwards(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
    int32 Current = NodeIndex;
    while (Current != INDEX_NONE)
    {
        RefitNode(ObjectBounds, Current);
        Current = Nodes[Current].Parent;
    }
}

// ============================================================================
// Rotation Optimization
// ============================================================================

/**
 * @brief Recompute `Depth` for a subtree after a structural change.
 * @param NodeIndex Root of the subtree whose depths should be rebuilt.
 * @param Depth Depth value assigned to `NodeIndex`.
 */
void FBVH::UpdateDepthsFromNode(int32 NodeIndex, int32 Depth)
{
    if (NodeIndex == INDEX_NONE)
    {
        return;
    }

    Nodes[NodeIndex].Depth = Depth;

    if (Nodes[NodeIndex].Left != INDEX_NONE)
    {
        UpdateDepthsFromNode(Nodes[NodeIndex].Left, Depth + 1);
    }

    if (Nodes[NodeIndex].Right != INDEX_NONE)
    {
        UpdateDepthsFromNode(Nodes[NodeIndex].Right, Depth + 1);
    }
}

/**
 * @brief Evaluate rotations that swap `Node.Right` with one grandchild under `Node.Left`.
 * @param NodeIndex Internal node whose local configuration will be evaluated.
 * @return The best beneficial rotation candidate, or an invalid candidate if none improves cost.
 */
FBVH::FRotationCandidate FBVH::EvaluateRotateWithLeftChild(int32 NodeIndex) const
{
    FRotationCandidate Best{};

    const FNode& Node = Nodes[NodeIndex];
    if (Node.IsLeaf() || Node.Left == INDEX_NONE || Node.Right == INDEX_NONE)
    {
        return Best;
    }

    const int32  AIndex = Node.Left;
    const int32  BIndex = Node.Right;
    const FNode& A = Nodes[AIndex];

    if (A.IsLeaf())
    {
        return Best;
    }

    const float OldCost = ComputeSurfaceArea(Node.Bounds) + ComputeSurfaceArea(A.Bounds);

    // Candidate 1: swap B with A.Left.
    {
        const int32 G = A.Left;
        const int32 O = A.Right;

        const FAABB NewA = UnionBounds(Nodes[BIndex].Bounds, Nodes[O].Bounds);
        const FAABB NewNode = UnionBounds(NewA, Nodes[G].Bounds);
        const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewA);

        if (!Best.bValid || NewCost < Best.NewCost)
        {
            Best.bValid = true;
            Best.bRotateLeftChild = true;
            Best.bUseFirstGrandChild = true; // Select A.Left.
            Best.NodeIndex = NodeIndex;
            Best.OldCost = OldCost;
            Best.NewCost = NewCost;
        }
    }

    // Candidate 2: swap B with A.Right.
    {
        const int32 G = A.Right;
        const int32 O = A.Left;

        const FAABB NewA = UnionBounds(Nodes[BIndex].Bounds, Nodes[O].Bounds);
        const FAABB NewNode = UnionBounds(NewA, Nodes[G].Bounds);
        const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewA);

        if (!Best.bValid || NewCost < Best.NewCost)
        {
            Best.bValid = true;
            Best.bRotateLeftChild = true;
            Best.bUseFirstGrandChild = false; // Select A.Right.
            Best.NodeIndex = NodeIndex;
            Best.OldCost = OldCost;
            Best.NewCost = NewCost;
        }
    }

    if (Best.bValid && Best.NewCost >= Best.OldCost)
    {
        Best.bValid = false;
    }

    return Best;
}

/**
 * @brief Evaluate rotations that swap `Node.Left` with one grandchild under `Node.Right`.
 * @param NodeIndex Internal node whose local configuration will be evaluated.
 * @return The best beneficial rotation candidate, or an invalid candidate if none improves cost.
 */
FBVH::FRotationCandidate FBVH::EvaluateRotateWithRightChild(int32 NodeIndex) const
{
    FRotationCandidate Best{};

    const FNode& Node = Nodes[NodeIndex];
    if (Node.IsLeaf() || Node.Left == INDEX_NONE || Node.Right == INDEX_NONE)
    {
        return Best;
    }

    const int32  AIndex = Node.Left;
    const int32  BIndex = Node.Right;
    const FNode& B = Nodes[BIndex];

    if (B.IsLeaf())
    {
        return Best;
    }

    const float OldCost = ComputeSurfaceArea(Node.Bounds) + ComputeSurfaceArea(B.Bounds);

    // Candidate 1: swap A with B.Left.
    {
        const int32 G = B.Left;
        const int32 O = B.Right;

        const FAABB NewB = UnionBounds(Nodes[AIndex].Bounds, Nodes[O].Bounds);
        const FAABB NewNode = UnionBounds(Nodes[G].Bounds, NewB);
        const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewB);

        if (!Best.bValid || NewCost < Best.NewCost)
        {
            Best.bValid = true;
            Best.bRotateLeftChild = false;
            Best.bUseFirstGrandChild = true; // Select B.Left.
            Best.NodeIndex = NodeIndex;
            Best.OldCost = OldCost;
            Best.NewCost = NewCost;
        }
    }

    // Candidate 2: swap A with B.Right.
    {
        const int32 G = B.Right;
        const int32 O = B.Left;

        const FAABB NewB = UnionBounds(Nodes[AIndex].Bounds, Nodes[O].Bounds);
        const FAABB NewNode = UnionBounds(Nodes[G].Bounds, NewB);
        const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewB);

        if (!Best.bValid || NewCost < Best.NewCost)
        {
            Best.bValid = true;
            Best.bRotateLeftChild = false;
            Best.bUseFirstGrandChild = false; // Select B.Right.
            Best.NodeIndex = NodeIndex;
            Best.OldCost = OldCost;
            Best.NewCost = NewCost;
        }
    }

    if (Best.bValid && Best.NewCost >= Best.OldCost)
    {
        Best.bValid = false;
    }

    return Best;
}

/**
 * @brief Apply one previously evaluated local rotation.
 * @param ObjectBounds Updated per-object AABBs.
 * @param Candidate Rotation candidate selected by `TryRotateNodeBest`.
 * @return `true` if the candidate was valid and the rotation was applied.
 *
 * The rotation only rewires local parent/child links. Leaves keep ownership of
 * the same object indices, so `ObjectToLeafNode` stays valid.
 */
bool FBVH::ApplyRotation(const TArray<FAABB>& ObjectBounds, const FRotationCandidate& Candidate)
{
    if (!Candidate.bValid || Candidate.NodeIndex == INDEX_NONE)
    {
        return false;
    }

    FNode& Node = Nodes[Candidate.NodeIndex];

    if (Candidate.bRotateLeftChild)
    {
        // Local shape before rotation: Node = (A, B), where A is internal.
        const int32 AIndex = Node.Left;
        const int32 BIndex = Node.Right;
        FNode&      A = Nodes[AIndex];

        if (Candidate.bUseFirstGrandChild)
        {
            Node.Right = A.Left;
            A.Left = BIndex;
        }
        else
        {
            Node.Right = A.Right;
            A.Right = BIndex;
        }

        Nodes[Node.Right].Parent = Candidate.NodeIndex;
        Nodes[BIndex].Parent = AIndex;
        A.Parent = Candidate.NodeIndex;

        // Only the subtree rooted at Node changes depth values.
        UpdateDepthsFromNode(Candidate.NodeIndex, Nodes[Candidate.NodeIndex].Depth);

        // Refit from the rotated child upward instead of refitting the whole tree.
        RefitUpwards(ObjectBounds, AIndex);
    }
    else
    {
        // Local shape before rotation: Node = (A, B), where B is internal.
        const int32 AIndex = Node.Left;
        const int32 BIndex = Node.Right;
        FNode&      B = Nodes[BIndex];

        if (Candidate.bUseFirstGrandChild)
        {
            Node.Left = B.Left;
            B.Left = AIndex;
        }
        else
        {
            Node.Left = B.Right;
            B.Right = AIndex;
        }

        Nodes[Node.Left].Parent = Candidate.NodeIndex;
        Nodes[AIndex].Parent = BIndex;
        B.Parent = Candidate.NodeIndex;

        UpdateDepthsFromNode(Candidate.NodeIndex, Nodes[Candidate.NodeIndex].Depth);
        RefitUpwards(ObjectBounds, BIndex);
    }

    return true;
}

/**
 * @brief Pick the better of the left-child and right-child rotation candidates.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Internal node to optimize locally.
 * @return `true` if a rotation was applied.
 */
bool FBVH::TryRotateNodeBest(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
    const FRotationCandidate LeftCandidate = EvaluateRotateWithLeftChild(NodeIndex);
    const FRotationCandidate RightCandidate = EvaluateRotateWithRightChild(NodeIndex);

    FRotationCandidate Best{};

    if (LeftCandidate.bValid)
    {
        Best = LeftCandidate;
    }

    if (RightCandidate.bValid && (!Best.bValid || RightCandidate.NewCost < Best.NewCost))
    {
        Best = RightCandidate;
    }

    if (!Best.bValid)
    {
        return false;
    }

    return ApplyRotation(ObjectBounds, Best);
}

/**
 * @brief Perform several bottom-up local rotation passes over the active BVH.
 * @param ObjectBounds Updated per-object AABBs.
 * @note Reachable nodes are recollected every pass because successful rotations
 * can change which indices lie on each bottom-up path.
 */
void FBVH::RotationBVH(const TArray<FAABB>& ObjectBounds)
{
    if (RootNodeIndex == INDEX_NONE || Nodes.empty())
    {
        return;
    }

    // Start from consistent bounds before comparing local surface-area costs.
    RefitBVHFull(ObjectBounds);
    BVH_VALIDATE();

    bool            bChanged = true;
    int32           PassCount = 0;
    constexpr int32 MaxPasses = 4;

    while (bChanged && PassCount < MaxPasses)
    {
        bChanged = false;
        ++PassCount;

        CollectReachableNodes(ReachableNodeIndicesScratch);

        for (int32 OrderIndex = static_cast<int32>(ReachableNodeIndicesScratch.size()) - 1; OrderIndex >= 0;
             --OrderIndex)
        {
            if (TryRotateNodeBest(ObjectBounds, ReachableNodeIndicesScratch[OrderIndex]))
            {
                bChanged = true;
            }
        }

        BVH_VALIDATE();
    }
}

// ============================================================================
// Dynamic Object Update
// ============================================================================

/**
 * @brief Insert a single object into an existing BVH.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param ObjectIndex Object index to insert.
 * @return Inserted leaf node index, or `INDEX_NONE` if the index is invalid or already present.
 */
int32 FBVH::InsertObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectBounds.size()))
    {
        return INDEX_NONE;
    }

    const int32 ExistingLeafNodeIndex = FindLeafNodeIndexByObject(ObjectIndex);
    if (ExistingLeafNodeIndex != INDEX_NONE)
    {
        return INDEX_NONE;
    }

    const int32 LeafNodeIndex = InsertLeafNode(ObjectBounds, ObjectIndex);
    if (LeafNodeIndex != INDEX_NONE)
    {
        BVH_VALIDATE();
    }

    return LeafNodeIndex;
}

/**
 * @brief Remove a single object from the BVH.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param ObjectIndex Object index to remove.
 * @return `true` if the object existed and was removed.
 */
bool FBVH::RemoveObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return false;
    }

    const int32 LeafNodeIndex = FindLeafNodeIndexByObject(ObjectIndex);
    if (LeafNodeIndex == INDEX_NONE)
    {
        return false;
    }

    RemoveLeafNode(ObjectBounds, LeafNodeIndex);
    BVH_VALIDATE();
    return true;
}

/**
 * @brief Update a single leaf bounds and locally re-optimize the affected path.
 * @param ObjectBounds Updated per-object AABBs referenced by the BVH.
 * @param ObjectIndex Object index whose bounds changed.
 * @return `true` if the object existed and the BVH was updated.
 */
bool FBVH::UpdateObject(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectBounds.size()) ||
        ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return false;
    }

    const int32 LeafNodeIndex = FindLeafNodeIndexByObject(ObjectIndex);
    if (LeafNodeIndex == INDEX_NONE)
    {
        return false;
    }

    FNode& LeafNode = Nodes[LeafNodeIndex];
    LeafNode.Bounds = ObjectBounds[ObjectIndex];
    RefitUpwards(ObjectBounds, LeafNodeIndex);
    OptimizeAlongPath(ObjectBounds, LeafNodeIndex);

    BVH_VALIDATE();
    return true;
}

// ============================================================================
// Queries
// ============================================================================

/**
 * @brief Traverse BVH and collect object indices intersecting a frustum.
 * @param Frustum Query frustum.
 * @param OutIndices Output list of intersecting object indices.
 * @param Scratch Caller-owned traversal scratch.
 * @param bInsideOnly If true, only nodes fully inside the frustum are accepted.
 */
void FBVH::FrustumQuery(const FFrustum& Frustum, TArray<int32>& OutIndices, FFrustumQueryScratch& Scratch,
                        bool bInsideOnly) const
{
    OutIndices.clear();

    if (RootNodeIndex == INDEX_NONE)
    {
        return;
    }

    // The caller owns this scratch, so separate threads can query the same BVH
    // concurrently as long as they do not share the scratch instance.
    Scratch.TraversalStack.clear();
    Scratch.TraversalStack.reserve(Nodes.size());
    Scratch.TraversalStack.push_back({RootNodeIndex, false});

    while (!Scratch.TraversalStack.empty())
    {
        const FFrustumQueryScratch::FStackEntry Entry = Scratch.TraversalStack.back();
        Scratch.TraversalStack.pop_back();

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
            // Once a parent is fully inside the frustum, descendants can skip
            // repeated frustum tests and are treated as inside as well.
            Result = FFrustum::EFrustumIntersectResult::Inside;
        }

        if (Node.IsLeaf())
        {
            if (Node.ObjectIndex != INDEX_NONE && Node.ObjectIndex >= 0)
            {
                OutIndices.push_back(Node.ObjectIndex);
            }
            continue;
        }

        const bool bChildAssumeInside = (Entry.bAssumeInside || Result == FFrustum::EFrustumIntersectResult::Inside);

        if (Node.Left != INDEX_NONE)
        {
            Scratch.TraversalStack.push_back({Node.Left, bChildAssumeInside});
        }
        if (Node.Right != INDEX_NONE)
        {
            Scratch.TraversalStack.push_back({Node.Right, bChildAssumeInside});
        }
    }
}

/**
 * @brief Return the closest leaf-AABB hit along the ray.
 * @param Ray Query ray.
 * @param OutObjectIndex Closest intersected object index, or `INDEX_NONE` on miss.
 * @param OutT Ray distance to the closest leaf AABB hit.
 * @param Scratch Caller-owned traversal scratch.
 * @return `true` if any leaf AABB was hit.
 *
 * The traversal uses front-to-back ordering and prunes any subtree whose entry
 * distance is already farther than the best hit found so far.
 */
bool FBVH::RayQueryClosestAABB(const FRay& Ray, int32& OutObjectIndex, float& OutT, FRayQueryScratch& Scratch) const
{
    OutObjectIndex = INDEX_NONE;
    OutT = FLT_MAX;

    if (RootNodeIndex == INDEX_NONE)
    {
        return false;
    }

    Scratch.NodeStack.clear();
    Scratch.NodeStack.reserve(Nodes.size());

    float tRoot = 0.0f;
    if (!Nodes[RootNodeIndex].Bounds.IntersectRay(Ray, tRoot))
    {
        return false;
    }

    Scratch.NodeStack.push_back({RootNodeIndex, tRoot});

    while (!Scratch.NodeStack.empty())
    {
        const FRayQueryScratch::FStackEntry Entry = Scratch.NodeStack.back();
        Scratch.NodeStack.pop_back();

        if (Entry.TEnter > OutT)
        {
            continue;
        }

        const FNode& Node = Nodes[Entry.NodeIndex];
        if (Node.IsLeaf())
        {
            const int32 ObjIndex = Node.ObjectIndex;
            if (ObjIndex != INDEX_NONE && ObjIndex >= 0 && Entry.TEnter < OutT)
            {
                OutObjectIndex = ObjIndex;
                OutT = Entry.TEnter;
            }
            continue;
        }

        float      tL = FLT_MAX;
        float      tR = FLT_MAX;
        const bool bL = (Node.Left != INDEX_NONE) && Nodes[Node.Left].Bounds.IntersectRay(Ray, tL) && (tL <= OutT);
        const bool bR = (Node.Right != INDEX_NONE) && Nodes[Node.Right].Bounds.IntersectRay(Ray, tR) && (tR <= OutT);

        // Push the farther child first so the next pop visits the nearer child first.
        if (bL && bR)
        {
            if (tL < tR)
            {
                Scratch.NodeStack.push_back({Node.Right, tR});
                Scratch.NodeStack.push_back({Node.Left, tL});
            }
            else
            {
                Scratch.NodeStack.push_back({Node.Left, tL});
                Scratch.NodeStack.push_back({Node.Right, tR});
            }
        }
        else if (bL)
        {
            Scratch.NodeStack.push_back({Node.Left, tL});
        }
        else if (bR)
        {
            Scratch.NodeStack.push_back({Node.Right, tR});
        }
    }

    return OutObjectIndex != INDEX_NONE;
}

/**
 * @brief Traverse BVH and collect ray-hit object indices and hit distances.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param Ray Query ray.
 * @param OutIndices Output hit object indices.
 * @param OutTs Output hit distances aligned with OutIndices.
 * @param Scratch Caller-owned traversal and sorting scratch.
 */
void FBVH::RayQuery(const TArray<FAABB>& ObjectBounds, const FRay& Ray, TArray<int32>& OutIndices,
                    TArray<float>& OutTs, FRayQueryScratch& Scratch) const
{
    OutIndices.clear();
    OutTs.clear();

    if (RootNodeIndex == INDEX_NONE)
    {
        return;
    }

    // The caller owns this scratch, so separate threads can query the same BVH
    // concurrently as long as they do not share the scratch instance.
    Scratch.NodeStack.clear();
    Scratch.NodeStack.reserve(Nodes.size());

    float tRoot = 0.f;
    if (!Nodes[RootNodeIndex].Bounds.IntersectRay(Ray, tRoot))
        return;
    Scratch.NodeStack.push_back({RootNodeIndex, tRoot});

    while (!Scratch.NodeStack.empty())
    {
        const FRayQueryScratch::FStackEntry Entry = Scratch.NodeStack.back();
        Scratch.NodeStack.pop_back();

        const int32 NodeIndex = Entry.NodeIndex;
        const FNode& Node = Nodes[NodeIndex];

        if (Node.IsLeaf())
        {
            const int32 ObjIndex = Node.ObjectIndex;
            if (ObjIndex != INDEX_NONE && ObjIndex >= 0 && ObjIndex < static_cast<int32>(ObjectBounds.size()))
            {
                // The traversal already intersected the leaf bounds and carried
                // the corresponding entry distance to this point.
                OutIndices.push_back(ObjIndex);
                OutTs.push_back(Entry.TEnter);
            }
            continue;
        }

        float      tL = FLT_MAX, tR = FLT_MAX;
        const bool bL = (Node.Left != INDEX_NONE) && Nodes[Node.Left].Bounds.IntersectRay(Ray, tL);
        const bool bR = (Node.Right != INDEX_NONE) && Nodes[Node.Right].Bounds.IntersectRay(Ray, tR);

        // Push the farther child first so the LIFO stack visits the nearer child first.
        // This keeps the hit list close to sorted order and makes the final reorder cheaper.
        if (bL && bR)
        {
            if (tL < tR)
            {
                Scratch.NodeStack.push_back({Node.Right, tR});
                Scratch.NodeStack.push_back({Node.Left, tL});
            }
            else
            {
                Scratch.NodeStack.push_back({Node.Left, tL});
                Scratch.NodeStack.push_back({Node.Right, tR});
            }
        }
        else if (bL)
        {
            Scratch.NodeStack.push_back({Node.Left, tL});
        }
        else if (bR)
        {
            Scratch.NodeStack.push_back({Node.Right, tR});
        }
    }

    if (OutIndices.size() <= 1)
        return; // Sorting is unnecessary for 0 or 1 hit.

    // Front-to-back traversal leaves hits nearly sorted in many cases.
    // Use an in-place insertion sort for small hit sets to avoid extra
    // permutation and copy passes.
    const int32 N = static_cast<int32>(OutIndices.size());
    constexpr int32 InsertionSortThreshold = 32;

    if (N <= InsertionSortThreshold)
    {
        for (int32 i = 1; i < N; ++i)
        {
            const float KeyT = OutTs[i];
            const int32 KeyIndex = OutIndices[i];

            int32 j = i - 1;
            while (j >= 0 && OutTs[j] > KeyT)
            {
                OutTs[j + 1] = OutTs[j];
                OutIndices[j + 1] = OutIndices[j];
                --j;
            }

            OutTs[j + 1] = KeyT;
            OutIndices[j + 1] = KeyIndex;
        }
        return;
    }

    // Larger hit sets still use the scratch-backed permutation path so the
    // public outputs end up strictly sorted without allocating per call.
    Scratch.Order.resize(N);
    for (int32 i = 0; i < N; ++i)
        Scratch.Order[i] = i;

    std::sort(Scratch.Order.begin(), Scratch.Order.end(), [&](int32 A, int32 B) { return OutTs[A] < OutTs[B]; });

    Scratch.SortedIndices.clear();
    Scratch.SortedIndices.reserve(N);
    Scratch.SortedTs.clear();
    Scratch.SortedTs.reserve(N);
    for (int32 Ord : Scratch.Order)
    {
        Scratch.SortedIndices.push_back(OutIndices[Ord]);
        Scratch.SortedTs.push_back(OutTs[Ord]);
    }
    OutIndices.swap(Scratch.SortedIndices);
    OutTs.swap(Scratch.SortedTs);
}


// ============================================================================
// Build Helpers
// ============================================================================

/**
 * @brief Recursively build a BVH node for the given object range.
 * @param ObjectBounds Array of per-object AABBs.
 * @param BuildObjectIndices Build-local indirection array into `ObjectBounds`.
 * @param Start Start index into `BuildObjectIndices`.
 * @param Count Number of objects in the range.
 * @return Index of the newly created node.
 */
int32 FBVH::BuildNode(const TArray<FAABB>& ObjectBounds, TArray<int32>& BuildObjectIndices, int32 Start, int32 Count,
                     int32 Depth)
{
    assert(Count > 0);

    const int32 NodeIndex{static_cast<int32>(Nodes.size())};
    Nodes.emplace_back();

    Nodes[NodeIndex].Depth = Depth;
    Nodes[NodeIndex].Bounds = ComputeBounds(ObjectBounds, BuildObjectIndices, Start, Count);

    // One object per leaf: once the range size is 1, the build stops here.
    if (Count == 1)
    {
        const int32 ObjIndex = BuildObjectIndices[Start];
        Nodes[NodeIndex].ObjectIndex = ObjIndex;
        LinkObjectToLeaf(ObjIndex, NodeIndex);
        return NodeIndex;
    }

    // Split the current range using SAH when possible.
    const FSplitCriterion SplitCriterion{FindSplitPosition(ObjectBounds, BuildObjectIndices, Start, Count)};
    int32                 SplitMid{PartitionObjects(ObjectBounds, BuildObjectIndices, Start, Count, SplitCriterion)};

    int32 LeftCount{SplitMid - Start};
    int32 RightCount{Count - LeftCount};

    // Fall back to a median split if every object lands on the same side.
    if (LeftCount <= 0 || RightCount <= 0)
    {
        SplitMid = Start + Count / 2;
        LeftCount = SplitMid - Start;
        RightCount = Count - LeftCount;
        assert(LeftCount > 0 && RightCount > 0);
    }

    Nodes[NodeIndex].Left = BuildNode(ObjectBounds, BuildObjectIndices, Start, LeftCount, Depth + 1);
    Nodes[NodeIndex].Right = BuildNode(ObjectBounds, BuildObjectIndices, SplitMid, RightCount, Depth + 1);

    Nodes[Nodes[NodeIndex].Left].Parent = NodeIndex;
    Nodes[Nodes[NodeIndex].Right].Parent = NodeIndex;
    Nodes[NodeIndex].ObjectIndex = INDEX_NONE;

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
FAABB FBVH::ComputeBounds(const TArray<FAABB>& ObjectBounds, const TArray<int32>& ObjectIndices, int32 Start,
                          int32 Count)
{
    assert(Count > 0);

    FAABB Result = ObjectBounds[ObjectIndices[Start]];

    for (int32 i = 1; i < Count; ++i)
    {
        Result.ExpandToInclude(ObjectBounds[ObjectIndices[Start + i]]);
    }
    return Result;
}

// ============================================================================
// Split Helpers
// ============================================================================

/**
 * @brief Evaluate SAH candidates and pick a split axis/position.
 * @param ObjectBounds Array of per-object AABBs.
 * @param BuildObjectIndices Build-local indirection array into `ObjectBounds`.
 * @param Start Start index into `BuildObjectIndices`.
 * @param Count Number of objects in the range.
 * @return Best split criterion for this range.
 */
FBVH::FSplitCriterion FBVH::FindSplitPosition(const TArray<FAABB>& ObjectBounds,
                                              const TArray<int32>& BuildObjectIndices, int32 Start, int32 Count)
{
    assert(Count >= 2);

    auto ComputeSurfaceArea = [](const FAABB& Box) -> float
    {
        const FVector Extent = Box.Max - Box.Min;
        return 2.0f * (Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X);
    };

    const FAABB NodeBounds = ComputeBounds(ObjectBounds, BuildObjectIndices, Start, Count);
    const float ParentArea = ComputeSurfaceArea(NodeBounds);

    // If parent area is degenerate, SAH loses meaning; use fallback split.
    if (ParentArea <= MathUtil::Epsilon)
    {
        return FindSplitPositionFromBounds(NodeBounds);
    }

    FSplitCriterion Best = FindSplitPositionFromBounds(NodeBounds);
    float           BestCost = std::numeric_limits<float>::infinity();
    int32           BestBalance = std::numeric_limits<int32>::max();
    bool            bFound = false;

    // Reuse the same scratch buffers for every axis so deep builds do not keep
    // reallocating center lists and prefix/suffix bounds.
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        BuildAxisEntriesScratch.resize(Count);
        BuildPrefixBoundsScratch.resize(Count);
        BuildSuffixBoundsScratch.resize(Count);

        for (int32 i = 0; i < Count; ++i)
        {
            const int32 ObjIndex = BuildObjectIndices[Start + i];
            const float Center = ObjectBounds[ObjIndex].GetCenter()[Axis];
            BuildAxisEntriesScratch[i] = {ObjIndex, Center};
        }

        std::sort(BuildAxisEntriesScratch.begin(), BuildAxisEntriesScratch.end(),
                  [](const FBuildAxisEntry& A, const FBuildAxisEntry& B)
                  {
                      if (A.Center == B.Center)
                      {
                          return A.Index < B.Index;
                      }
                      return A.Center < B.Center;
                  });

        BuildPrefixBoundsScratch[0] = ObjectBounds[BuildAxisEntriesScratch[0].Index];
        for (int32 i = 1; i < Count; ++i)
        {
            BuildPrefixBoundsScratch[i] = BuildPrefixBoundsScratch[i - 1];
            BuildPrefixBoundsScratch[i].ExpandToInclude(ObjectBounds[BuildAxisEntriesScratch[i].Index]);
        }

        BuildSuffixBoundsScratch[Count - 1] = ObjectBounds[BuildAxisEntriesScratch[Count - 1].Index];
        for (int32 i = Count - 2; i >= 0; --i)
        {
            BuildSuffixBoundsScratch[i] = BuildSuffixBoundsScratch[i + 1];
            BuildSuffixBoundsScratch[i].ExpandToInclude(ObjectBounds[BuildAxisEntriesScratch[i].Index]);
        }

        for (int32 i = 0; i < Count - 1; ++i)
        {
            const float CenterA = BuildAxisEntriesScratch[i].Center;
            const float CenterB = BuildAxisEntriesScratch[i + 1].Center;

            // Skip split candidates between nearly identical centers.
            if (std::abs(CenterA - CenterB) <= MathUtil::Epsilon)
            {
                continue;
            }

            const int32 LeftCount = i + 1;
            const int32 RightCount = Count - LeftCount;

            const float LeftArea = ComputeSurfaceArea(BuildPrefixBoundsScratch[i]);
            const float RightArea = ComputeSurfaceArea(BuildSuffixBoundsScratch[i + 1]);

            // ParentArea is constant for all candidates, so omit it in comparison.
            const float Cost = LeftArea * static_cast<float>(LeftCount) + RightArea * static_cast<float>(RightCount);

            const int32 Balance = std::abs(LeftCount - RightCount);

            // 1) Prefer lower SAH cost.
            // 2) If costs are near-equal, prefer more balanced partitions.
            if (!bFound || Cost < BestCost - MathUtil::Epsilon ||
                (std::abs(Cost - BestCost) <= MathUtil::Epsilon && Balance < BestBalance))
            {
                bFound = true;
                BestCost = Cost;
                BestBalance = Balance;
                Best.Axis = static_cast<EBVHAxis>(Axis);
                Best.Position = 0.5f * (CenterA + CenterB);
            }
        }
    }

    if (!bFound)
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
FBVH::FSplitCriterion FBVH::FindSplitPositionFromBounds(const FAABB& NodeAABB)
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
 * @param BuildObjectIndices Build-local indirection array into `ObjectBounds`.
 * @param Start Start index into `BuildObjectIndices`.
 * @param Count Number of objects in the range.
 * @param Criterion Split axis and position.
 * @return Index separating left and right partitions.
 */
int32 FBVH::PartitionObjects(const TArray<FAABB>& ObjectBounds, TArray<int32>& BuildObjectIndices, int32 Start,
                             int32 Count, const FSplitCriterion& Criterion)
{
    assert(Count > 0);

    int32 Axis{GetAxisValue(Criterion.Axis)};
    int32 Left{Start};
    int32 Right{Start + Count - 1};

    while (Left <= Right)
    {
        const FAABB&  LeftBox{ObjectBounds[BuildObjectIndices[Left]]};
        const FVector LeftCenter{LeftBox.GetCenter()};
        const float   LeftValue{LeftCenter[Axis]};

        if (LeftValue < Criterion.Position)
        {
            ++Left;
            continue;
        }

        const FAABB&  RightBox{ObjectBounds[BuildObjectIndices[Right]]};
        const FVector RightCenter{RightBox.GetCenter()};
        const float   RightValue{RightCenter[Axis]};

        if (RightValue >= Criterion.Position)
        {
            --Right;
            continue;
        }

        std::swap(BuildObjectIndices[Left], BuildObjectIndices[Right]);
        ++Left;
        --Right;
    }

    const int32 Mid{Left};

    // Prevent a degenerate split where every object ends up on the same side.
    if (Mid == Start || Mid == Start + Count)
    {
        const int32 ForcedMid{Start + Count / 2};

        std::sort(BuildObjectIndices.begin() + Start, BuildObjectIndices.begin() + Start + Count,
                  [&](int32 A, int32 B)
                  {
                      const FVector CenterA{ObjectBounds[A].GetCenter()};
                      const FVector CenterB{ObjectBounds[B].GetCenter()};
                      if (CenterA[Axis] == CenterB[Axis])
                      {
                          return A < B;
                      }
                      return CenterA[Axis] < CenterB[Axis];
                  });
        return ForcedMid;
    }
    return Mid;
}

// ============================================================================
// Dynamic Object Update Helpers
// ============================================================================

/**
 * @brief Acquire a node slot either from the free list or by growing `Nodes`.
 * @return Index of a writable node slot.
 * @note Slots are reused instead of erased from `Nodes` so every stored index
 * in parent/child links and object mappings remains stable.
 */
int32 FBVH::AllocateNode()
{
    if (!FreeNodeIndices.empty())
    {
        const int32 NodeIndex = FreeNodeIndices.back();
        FreeNodeIndices.pop_back();

        assert(NodeIndex >= 0 && NodeIndex < static_cast<int32>(Nodes.size()));
        Nodes[NodeIndex] = FNode{};
        return NodeIndex;
    }

    const int32 NodeIndex = static_cast<int32>(Nodes.size());
    Nodes.emplace_back();
    return NodeIndex;
}

/**
 * @brief Return a node slot to the free list after it becomes unreachable.
 * @param NodeIndex Raw storage index to release.
 * @note The slot stays inside `Nodes`; only its contents are reset and its
 * index is recorded for reuse by a future insertion.
 */
void FBVH::ReleaseNode(int32 NodeIndex)
{
    assert(NodeIndex >= 0 && NodeIndex < static_cast<int32>(Nodes.size()));
    Nodes[NodeIndex] = FNode{};
    FreeNodeIndices.push_back(NodeIndex);
}

/**
 * @brief Grow the object-to-leaf mapping table so that `ObjectIndex` is addressable.
 * @param ObjectIndex Object index that must fit inside `ObjectToLeafNode`.
 */
void FBVH::EnsureObjectMappingCapacity(int32 ObjectIndex)
{
    assert(ObjectIndex >= 0);

    if (ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        ObjectToLeafNode.resize(ObjectIndex + 1, INDEX_NONE);
    }
}

/**
 * @brief Record that a specific leaf now owns an object index.
 * @param ObjectIndex Object index stored by the leaf.
 * @param LeafNodeIndex Leaf node index that owns the object.
 */
void FBVH::LinkObjectToLeaf(int32 ObjectIndex, int32 LeafNodeIndex)
{
    assert(ObjectIndex >= 0);
    assert(LeafNodeIndex >= 0 && LeafNodeIndex < static_cast<int32>(Nodes.size()));

    const FNode& LeafNode = Nodes[LeafNodeIndex];
    assert(LeafNode.IsLeaf());
    assert(LeafNode.ObjectIndex == ObjectIndex);

    EnsureObjectMappingCapacity(ObjectIndex);
    ObjectToLeafNode[ObjectIndex] = LeafNodeIndex;
}

/**
 * @brief Clear an object-to-leaf mapping if it still matches the expected leaf.
 * @param ObjectIndex Object index whose mapping may be cleared.
 * @param ExpectedLeafNodeIndex Leaf index expected to own the mapping.
 *
 * The extra check prevents a stale caller from accidentally erasing a mapping
 * that has already been rebound to a different leaf.
 */
void FBVH::UnlinkObjectFromLeaf(int32 ObjectIndex, int32 ExpectedLeafNodeIndex)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return;
    }

    assert(ExpectedLeafNodeIndex == INDEX_NONE || ObjectToLeafNode[ObjectIndex] == INDEX_NONE ||
           ObjectToLeafNode[ObjectIndex] == ExpectedLeafNodeIndex);

    if (ExpectedLeafNodeIndex == INDEX_NONE || ObjectToLeafNode[ObjectIndex] == ExpectedLeafNodeIndex)
    {
        ObjectToLeafNode[ObjectIndex] = INDEX_NONE;
    }
}

/**
 * @brief Create one leaf node for a single object.
 * @param ObjectBounds Per-object AABBs.
 * @param ObjectIndex Object to place into the new leaf.
 * @return Index of the new leaf, or `INDEX_NONE` if `ObjectIndex` is invalid.
 *
 * This only initializes the node. Linking it into the tree and updating
 * `ObjectToLeafNode` happens later once insertion succeeds.
 */
int32 FBVH::CreateLeafNode(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex)
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectBounds.size()))
    {
        return INDEX_NONE;
    }

    const int32 LeafNodeIndex = AllocateNode();
    FNode&      Leaf = Nodes[LeafNodeIndex];

    Leaf.Bounds = ObjectBounds[ObjectIndex];
    Leaf.Parent = INDEX_NONE;
    Leaf.Left = INDEX_NONE;
    Leaf.Right = INDEX_NONE;
    Leaf.ObjectIndex = ObjectIndex;
    Leaf.Depth = 0;

    return LeafNodeIndex;
}

/**
 * @brief Resolve an object index directly through the validated object-to-leaf mapping.
 * @param ObjectIndex Object index to resolve.
 * @return The owning leaf index, or `INDEX_NONE` if the object is not linked.
 * @note This intentionally does not fall back to a tree traversal. The mapping
 * is treated as an invariant maintained by structural build/insert/remove code.
 */
int32 FBVH::FindLeafNodeIndexByObject(int32 ObjectIndex) const
{
    if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(ObjectToLeafNode.size()))
    {
        return INDEX_NONE;
    }

    const int32 MappedLeafNodeIndex = ObjectToLeafNode[ObjectIndex];
    if (MappedLeafNodeIndex == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    const bool bMappedLeafNodeIndexValid =
        MappedLeafNodeIndex >= 0 && MappedLeafNodeIndex < static_cast<int32>(Nodes.size());
    assert(bMappedLeafNodeIndexValid);
    if (!bMappedLeafNodeIndexValid)
    {
        return INDEX_NONE;
    }

    const FNode& MappedLeafNode = Nodes[MappedLeafNodeIndex];
    const bool bLeafMappingValid = MappedLeafNode.IsLeaf() && MappedLeafNode.ObjectIndex == ObjectIndex;
    assert(bLeafMappingValid);
    return bLeafMappingValid ? MappedLeafNodeIndex : INDEX_NONE;
}

/**
 * @brief Descend the active tree and choose the best sibling for a new leaf.
 * @param NewBounds Bounds of the object that will be inserted.
 * @return Node index that should be paired with the new leaf.
 *
 * The descent uses a local surface-area heuristic with inheritance cost, similar
 * to common dynamic-BVH insertion strategies.
 */
int32 FBVH::FindBestSibling(const FAABB& NewBounds) const
{
    if (RootNodeIndex == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    int32 Current = RootNodeIndex;

    while (!Nodes[Current].IsLeaf())
    {
        const FNode& Node = Nodes[Current];
        assert(Node.Left != INDEX_NONE && Node.Right != INDEX_NONE);
        assert(Node.Left >= 0 && Node.Left < static_cast<int32>(Nodes.size()));
        assert(Node.Right >= 0 && Node.Right < static_cast<int32>(Nodes.size()));

        if (Node.Left == INDEX_NONE || Node.Right == INDEX_NONE || Node.Left < 0 ||
            Node.Left >= static_cast<int32>(Nodes.size()) || Node.Right < 0 ||
            Node.Right >= static_cast<int32>(Nodes.size()))
        {
            return Current;
        }

        const FNode& Left = Nodes[Node.Left];
        const FNode& Right = Nodes[Node.Right];

        const float NodeArea = ComputeSurfaceArea(Node.Bounds);
        const FAABB CombinedBounds = UnionBounds(Node.Bounds, NewBounds);
        const float CombinedArea = ComputeSurfaceArea(CombinedBounds);

        const float CostHere = 2.0f * CombinedArea;
        const float InheritanceCost = 2.0f * (CombinedArea - NodeArea);

        auto ComputeDescendCost = [&](const FNode& Child) -> float
        {
            const FAABB CombinedChildBounds = UnionBounds(Child.Bounds, NewBounds);
            const float CombinedChildArea = ComputeSurfaceArea(CombinedChildBounds);

            if (Child.IsLeaf())
            {
                return CombinedChildArea + InheritanceCost;
            }

            return (CombinedChildArea - ComputeSurfaceArea(Child.Bounds)) + InheritanceCost;
        };

        const float CostLeft = ComputeDescendCost(Left);
        const float CostRight = ComputeDescendCost(Right);

        if (CostHere <= CostLeft && CostHere <= CostRight)
        {
            // Pairing the new leaf with the current subtree root is cheaper
            // than descending further into either child.
            break;
        }

        Current = (CostLeft <= CostRight) ? Node.Left : Node.Right;
    }

    return Current;
}

/**
 * @brief Insert a new single-object leaf into the hierarchy.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param ObjectIndex Object to insert.
 * @return The new leaf index, or `INDEX_NONE` on failure.
 */
int32 FBVH::InsertLeafNode(const TArray<FAABB>& ObjectBounds, int32 ObjectIndex)
{
    const int32 NewLeafIndex = CreateLeafNode(ObjectBounds, ObjectIndex);
    if (NewLeafIndex == INDEX_NONE)
    {
        return INDEX_NONE;
    }

    // In an empty tree the inserted leaf becomes the root directly.
    if (RootNodeIndex == INDEX_NONE)
    {
        RootNodeIndex = NewLeafIndex;
        Nodes[NewLeafIndex].Depth = 0;
        LinkObjectToLeaf(ObjectIndex, NewLeafIndex);
        return NewLeafIndex;
    }

    const int32 SiblingIndex = FindBestSibling(Nodes[NewLeafIndex].Bounds);
    if (SiblingIndex == INDEX_NONE)
    {
        ReleaseNode(NewLeafIndex);
        return INDEX_NONE;
    }

    // Create a new parent that groups the old sibling subtree and the new leaf.
    const int32 OldParentIndex = Nodes[SiblingIndex].Parent;
    const int32 NewParentIndex = AllocateNode();

    FNode& NewParent = Nodes[NewParentIndex];
    NewParent.Parent = OldParentIndex;
    NewParent.Left = SiblingIndex;
    NewParent.Right = NewLeafIndex;
    NewParent.ObjectIndex = INDEX_NONE;
    NewParent.Bounds = UnionBounds(Nodes[SiblingIndex].Bounds, Nodes[NewLeafIndex].Bounds);

    if (OldParentIndex == INDEX_NONE)
    {
        RootNodeIndex = NewParentIndex;
        NewParent.Depth = 0;
    }
    else
    {
        NewParent.Depth = Nodes[OldParentIndex].Depth + 1;

        if (Nodes[OldParentIndex].Left == SiblingIndex)
        {
            Nodes[OldParentIndex].Left = NewParentIndex;
        }
        else
        {
            Nodes[OldParentIndex].Right = NewParentIndex;
        }
    }

    Nodes[SiblingIndex].Parent = NewParentIndex;
    Nodes[NewLeafIndex].Parent = NewParentIndex;

    // Rebuild depth/bounds locally, then try to recover tree quality along the affected path.
    UpdateDepthsFromNode(NewParentIndex, NewParent.Depth);
    RefitUpwardsAfterStructuralChange(ObjectBounds, NewParentIndex);
    OptimizeAlongPath(ObjectBounds, NewParentIndex);

    LinkObjectToLeaf(ObjectIndex, NewLeafIndex);

    return NewLeafIndex;
}

/**
 * @brief Remove one leaf from the hierarchy and promote its sibling.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param LeafNodeIndex Leaf node to remove.
 */
void FBVH::RemoveLeafNode(const TArray<FAABB>& ObjectBounds, int32 LeafNodeIndex)
{
    if (LeafNodeIndex == INDEX_NONE)
    {
        return;
    }

    FNode&      Leaf = Nodes[LeafNodeIndex];
    const int32 ParentIndex = Leaf.Parent;

    // If the only node is a leaf, removing it simply empties the hierarchy.
    if (ParentIndex == INDEX_NONE)
    {
        if (Leaf.ObjectIndex != INDEX_NONE)
        {
            UnlinkObjectFromLeaf(Leaf.ObjectIndex, LeafNodeIndex);
        }

        RootNodeIndex = INDEX_NONE;
        ReleaseNode(LeafNodeIndex);
        return;
    }

    FNode&      Parent = Nodes[ParentIndex];
    const int32 GrandParentIndex = Parent.Parent;
    const int32 SiblingIndex = (Parent.Left == LeafNodeIndex) ? Parent.Right : Parent.Left;

    // Clear the object mapping before releasing the leaf storage.
    if (Leaf.ObjectIndex != INDEX_NONE)
    {
        UnlinkObjectFromLeaf(Leaf.ObjectIndex, LeafNodeIndex);
    }

    if (GrandParentIndex == INDEX_NONE)
    {
        RootNodeIndex = SiblingIndex;
        Nodes[SiblingIndex].Parent = INDEX_NONE;
        UpdateDepthsFromNode(SiblingIndex, 0);
    }
    else
    {
        if (Nodes[GrandParentIndex].Left == ParentIndex)
        {
            Nodes[GrandParentIndex].Left = SiblingIndex;
        }
        else
        {
            Nodes[GrandParentIndex].Right = SiblingIndex;
        }

        Nodes[SiblingIndex].Parent = GrandParentIndex;
        UpdateDepthsFromNode(SiblingIndex, Nodes[GrandParentIndex].Depth + 1);
    }

    const int32 RefitStart = Nodes[SiblingIndex].Parent;

    // The removed leaf and its obsolete parent become reusable free-list entries.
    ReleaseNode(LeafNodeIndex);
    ReleaseNode(ParentIndex);

    if (RefitStart != INDEX_NONE)
    {
        RefitUpwardsAfterStructuralChange(ObjectBounds, RefitStart);
        OptimizeAlongPath(ObjectBounds, RefitStart);
    }
}

/**
 * @brief Refit from a topology-change point to the root without rebuilding dirty lists.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param NodeIndex First node whose bounds may need recomputation.
 * @note This is used after insert/remove rewires links. The normal incremental
 * refit path assumes stable topology and starts from dirty object indices.
 */
void FBVH::RefitUpwardsAfterStructuralChange(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
    int32 Current = NodeIndex;
    while (Current != INDEX_NONE)
    {
        FNode& Node = Nodes[Current];

        if (Node.IsLeaf())
        {
            if (Node.ObjectIndex != INDEX_NONE && Node.ObjectIndex >= 0 &&
                Node.ObjectIndex < static_cast<int32>(ObjectBounds.size()))
            {
                Node.Bounds = ObjectBounds[Node.ObjectIndex];
            }
        }
        else
        {
            Node.Bounds = UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
            Node.ObjectIndex = INDEX_NONE;
        }

        Current = Node.Parent;
    }
}

/**
 * @brief Try local rotations along the path from one changed node to the root.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param StartNodeIndex First node on the affected path.
 *
 * The path is collected before rotations begin so parent-link rewrites do not
 * interfere with the set of nodes selected for local optimization.
 */
void FBVH::OptimizeAlongPath(const TArray<FAABB>& ObjectBounds, int32 StartNodeIndex)
{
    PathToRootScratch.clear();
    PathToRootScratch.reserve(32);

    int32 Current = StartNodeIndex;
    while (Current != INDEX_NONE)
    {
        PathToRootScratch.push_back(Current);
        Current = Nodes[Current].Parent;
    }

    for (int32 NodeIndex : PathToRootScratch)
    {
        while (TryRotateNodeBest(ObjectBounds, NodeIndex))
        {
            // Keep rotating the same node until no local improvement remains.
        }
    }
}

// ============================================================================
// Shared Helpers / Validation
// ============================================================================

/**
 * @brief Compute the surface area of an AABB.
 * @param Box Input AABB.
 * @return Surface area value.
 */
float FBVH::ComputeSurfaceArea(const FAABB& Box)
{
    const FVector Extent = Box.Max - Box.Min;
    return 2.0f * (Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X);
}

/**
 * @brief Compute the union bounds of two AABBs.
 * @param A First AABB.
 * @param B Second AABB.
 * @return Merged AABB that encloses both inputs.
 */
FAABB FBVH::UnionBounds(const FAABB& A, const FAABB& B)
{
    FAABB Result = A;
    Result.ExpandToInclude(B);
    return Result;
}

/**
 * @brief Validate parent/child topology invariants using assertions.
 * @note This function is intended for debug builds and is comparatively expensive.
 */
void FBVH::ValidateBVH() const
{
    if (RootNodeIndex == INDEX_NONE)
    {
        for (int32 ObjectIndex = 0; ObjectIndex < static_cast<int32>(ObjectToLeafNode.size()); ++ObjectIndex)
        {
            assert(ObjectToLeafNode[ObjectIndex] == INDEX_NONE);
        }

        TArray<uint8> bFreeNodeSeen;
        bFreeNodeSeen.resize(Nodes.size(), 0u);

        for (int32 FreeNodeIndex : FreeNodeIndices)
        {
            assert(FreeNodeIndex >= 0 && FreeNodeIndex < static_cast<int32>(Nodes.size()));
            assert(!bFreeNodeSeen[FreeNodeIndex]);
            bFreeNodeSeen[FreeNodeIndex] = 1u;

            const FNode& FreeNode = Nodes[FreeNodeIndex];
            assert(FreeNode.Parent == INDEX_NONE);
            assert(FreeNode.Left == INDEX_NONE);
            assert(FreeNode.Right == INDEX_NONE);
            assert(FreeNode.ObjectIndex == INDEX_NONE);
            assert(FreeNode.Depth == -1);
        }

        return;
    }

    assert(RootNodeIndex >= 0 && RootNodeIndex < static_cast<int32>(Nodes.size()));
    assert(Nodes[RootNodeIndex].Parent == INDEX_NONE);
    assert(Nodes[RootNodeIndex].Depth == 0);

    TArray<uint8> bReachable;
    bReachable.resize(Nodes.size(), 0u);

    TArray<int32> Stack;
    Stack.reserve(Nodes.size());
    Stack.push_back(RootNodeIndex);
    bReachable[RootNodeIndex] = 1u;

    while (!Stack.empty())
    {
        const int32 i = Stack.back();
        Stack.pop_back();

        const FNode& Node = Nodes[i];
        assert(Node.Depth >= 0);

        if (Node.IsLeaf())
        {
            // Active leaves never own children.
            assert(Node.Left == INDEX_NONE);
            assert(Node.Right == INDEX_NONE);

            // Active leaves must own exactly one valid object index.
            assert(Node.ObjectIndex != INDEX_NONE);
            assert(Node.ObjectIndex >= 0);
            assert(Node.ObjectIndex < static_cast<int32>(ObjectToLeafNode.size()));
            assert(ObjectToLeafNode[Node.ObjectIndex] == i);
        }
        else
        {
            // Active internal nodes must own exactly two distinct children.
            assert(Node.Left != INDEX_NONE);
            assert(Node.Right != INDEX_NONE);
            assert(Node.ObjectIndex == INDEX_NONE);

            assert(Node.Left >= 0 && Node.Left < static_cast<int32>(Nodes.size()));
            assert(Node.Right >= 0 && Node.Right < static_cast<int32>(Nodes.size()));
            assert(Node.Left != Node.Right);

            // Child-to-parent links and cached depths must agree in both directions.
            assert(Nodes[Node.Left].Parent == i);
            assert(Nodes[Node.Right].Parent == i);
            assert(Nodes[Node.Left].Depth == Node.Depth + 1);
            assert(Nodes[Node.Right].Depth == Node.Depth + 1);

            if (!bReachable[Node.Left])
            {
                bReachable[Node.Left] = 1u;
                Stack.push_back(Node.Left);
            }
            if (!bReachable[Node.Right])
            {
                bReachable[Node.Right] = 1u;
                Stack.push_back(Node.Right);
            }

            // Every internal node bound must exactly match the union of its children.
            const FAABB ExpectedBounds = UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
            const bool  bBoundsValid = FAABB::NearlyEqualAABB(Node.Bounds, ExpectedBounds);
            assert(bBoundsValid);
        }
    }

    for (int32 ObjectIndex = 0; ObjectIndex < static_cast<int32>(ObjectToLeafNode.size()); ++ObjectIndex)
    {
        const int32 LeafIndex = ObjectToLeafNode[ObjectIndex];
        if (LeafIndex == INDEX_NONE)
        {
            continue;
        }

        assert(LeafIndex >= 0 && LeafIndex < static_cast<int32>(Nodes.size()));
        assert(bReachable[LeafIndex]);
        assert(Nodes[LeafIndex].IsLeaf());
        assert(Nodes[LeafIndex].ObjectIndex == ObjectIndex);
    }

    TArray<uint8> bFreeNodeSeen;
    bFreeNodeSeen.resize(Nodes.size(), 0u);

    for (int32 FreeNodeIndex : FreeNodeIndices)
    {
        assert(FreeNodeIndex >= 0 && FreeNodeIndex < static_cast<int32>(Nodes.size()));
        assert(!bFreeNodeSeen[FreeNodeIndex]);
        bFreeNodeSeen[FreeNodeIndex] = 1u;

        assert(!bReachable[FreeNodeIndex]);

        const FNode& FreeNode = Nodes[FreeNodeIndex];
        assert(FreeNode.Parent == INDEX_NONE);
        assert(FreeNode.Left == INDEX_NONE);
        assert(FreeNode.Right == INDEX_NONE);
        assert(FreeNode.ObjectIndex == INDEX_NONE);
        assert(FreeNode.Depth == -1);
    }
}
