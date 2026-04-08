#include "BVH.h"

#include <cassert>
#include <algorithm>
#include <cfloat>

#include "Engine/Geometry/Frustum.h"
#include "Engine/Geometry/AABB.h"

/**
 * @file BVH.cpp
 * @brief FBVH build, refit, optimization, and query implementation.
 */

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
	for (int32 i{ 0 }; i < ObjectCount; ++i)
	{
		ObjectIndices[i] = i;
	}
	RootNodeIndex = BuildNode(ObjectBounds, 0, ObjectCount);

	ValidateBVH();
}

/**
 * @brief Build a BLAS using the BVH build routine.
 * @param ObjectBounds Array of per-object AABBs.
 * @param InLeafSize Maximum number of objects per leaf node.
 */
void FBVH::BuildBLAS(const TArray<FAABB>& ObjectBounds, int32 InLeafSize)
{
	BuildBVH(ObjectBounds, InLeafSize);
}

/**
 * @brief Refit all node bounds in an existing BVH.
 * @param ObjectBounds Updated per-object AABBs.
 */
void FBVH::RefitBVH(const TArray<FAABB>& ObjectBounds)
{
	if (RootNodeIndex == INDEX_NONE || Nodes.empty())
	{
		return;
	}

	for (int32 NodeIndex = static_cast<int32>(Nodes.size()) - 1; NodeIndex >= 0; --NodeIndex)
	{
		RefitNode(ObjectBounds, NodeIndex);
	}

	ValidateBVH();
}

/**
 * @brief Perform local rotations to reduce hierarchy cost, then refit.
 * @param ObjectBounds Updated per-object AABBs.
 */
void FBVH::RotationBVH(const TArray<FAABB>& ObjectBounds)
{
	if (RootNodeIndex == INDEX_NONE || Nodes.empty())
	{
		return;
	}

	RefitBVH(ObjectBounds);
	RebuildParentLinks();
	ValidateBVH();

	bool bChanged = true;
	int32 PassCount = 0;
	constexpr int32 MaxPasses = 4;

	while (bChanged && PassCount < MaxPasses)
	{
		bChanged = false;
		++PassCount;

		for (int32 NodeIndex = static_cast<int32>(Nodes.size()) - 1; NodeIndex >= 0; --NodeIndex)
		{
			if (TryRotateNode(ObjectBounds, NodeIndex))
			{
				bChanged = true;
			}
		}

		if (bChanged)
		{
			RefitBVH(ObjectBounds);
			RebuildParentLinks();
			ValidateBVH();
		}
	}
}

/**
 * @brief Rebuild BVH from scratch with a resolved leaf size.
 * @param ObjectBounds Per-object AABBs used to rebuild.
 * @param InLeafSize Requested leaf size. If <= 0, fallback to current/default.
 */
void FBVH::ReBuildBVH(const TArray<FAABB>& ObjectBounds, int32 InLeafSize)
{
	const int32 EffectiveLeafSize =
		(InLeafSize > 0) ? InLeafSize :
		((LeafSize > 0) ? LeafSize : 4);

	BuildBVH(ObjectBounds, EffectiveLeafSize);
}

/**
 * @brief Traverse BVH and collect object indices intersecting a frustum.
 * @param Frustum Query frustum.
 * @param OutIndices Output list of intersecting object indices.
 * @param bInsideOnly If true, only nodes fully inside the frustum are accepted.
 */
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
	Stack.push_back({ RootNodeIndex, false });

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
			Stack.push_back({ Node.Left, bChildAssumeInside });
		}
		if (Node.Right != INDEX_NONE)
		{
			Stack.push_back({ Node.Right, bChildAssumeInside });
		}
	}
}

/**
 * @brief Traverse BVH and collect ray-hit object indices and hit distances.
 * @param ObjectBounds Per-object AABBs referenced by the BVH.
 * @param Ray Query ray.
 * @param OutIndices Output hit object indices.
 * @param OutTs Output hit distances aligned with OutIndices.
 */
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
	Stack.push_back({ RootNodeIndex, tRoot });

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

		// Push the farther child first so LIFO pops the nearer child first.
		// This keeps traversal roughly front-to-back and reduces sort work later.
		if (bL && bR)
		{
			if (tL < tR)
			{
				Stack.push_back({ Node.Right, tR });
				Stack.push_back({ Node.Left, tL });
			}
			else
			{
				Stack.push_back({ Node.Left, tL });
				Stack.push_back({ Node.Right, tR });
			}
		}
		else if (bL)
			Stack.push_back({ Node.Left, tL });
		else if (bR)
			Stack.push_back({ Node.Right, tR });
	}

	if (OutIndices.size() <= 1)
		return; // Sorting is unnecessary for 0 or 1 hit.

	// Front-to-back traversal leaves hits nearly sorted in many cases.
	// For the expected small hit counts here, index sorting is sufficient.
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

	const int32 NodeIndex{ static_cast<int32>(Nodes.size()) };
	Nodes.emplace_back();

	Nodes[NodeIndex].Bounds = ComputeBounds(ObjectBounds, ObjectIndices, Start, Count);

	// Leaf node termination.
	if (Count <= LeafSize)
	{
		Nodes[NodeIndex].FirstObject = Start;
		Nodes[NodeIndex].ObjectCount = Count;
		return NodeIndex;
	}

	// SAH-based split.
	const FSplitCriterion SplitCriterion{ FindSplitPosition(ObjectBounds, Start, Count) };
	const int32           MidIndex{ PartitionObjects(ObjectBounds, Start, Count, SplitCriterion) };

	const int32 LeftCount{ MidIndex - Start };
	const int32 RightCount{ Count - LeftCount };

	// Safety net against degenerate partitioning.
	if (LeftCount <= 0 || RightCount <= 0)
	{
		Nodes[NodeIndex].FirstObject = Start;
		Nodes[NodeIndex].ObjectCount = Count;
		return NodeIndex;
	}

	Nodes[NodeIndex].Left = BuildNode(ObjectBounds, Start, LeftCount);
	Nodes[NodeIndex].Right = BuildNode(ObjectBounds, MidIndex, RightCount);

	Nodes[Nodes[NodeIndex].Left].Parent = NodeIndex;
	Nodes[Nodes[NodeIndex].Right].Parent = NodeIndex;

	// Internal nodes also track the full object range they cover.
	// During partitioning, child ranges remain contiguous in ObjectIndices.
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

/**
 * @brief Evaluate SAH candidates and pick a split axis/position.
 * @param ObjectBounds Array of per-object AABBs.
 * @param Start Start index into ObjectIndices.
 * @param Count Number of objects in the range.
 * @return Best split criterion for this range.
 */
FSplitCriterion FBVH::FindSplitPosition(const TArray<FAABB>& ObjectBounds, int32 Start, int32 Count)
{
	assert(Count >= 2);

	struct FAxisEntry
	{
		int32 Index;
		float Center;
	};

	auto ComputeSurfaceArea = [](const FAABB& Box) -> float
		{
			const FVector Extent = Box.Max - Box.Min;
			return 2.0f * (Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X);
		};

	const FAABB NodeBounds = ComputeBounds(ObjectBounds, ObjectIndices, Start, Count);
	const float ParentArea = ComputeSurfaceArea(NodeBounds);

	// If parent area is degenerate, SAH loses meaning; use fallback split.
	if (ParentArea <= MathUtil::Epsilon)
	{
		return FindSplitPositionFromBounds(NodeBounds);
	}

	FSplitCriterion Best = FindSplitPositionFromBounds(NodeBounds);
	float BestCost = std::numeric_limits<float>::infinity();
	int32 BestBalance = std::numeric_limits<int32>::max();
	bool bFound = false;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		TArray<FAxisEntry> Entries;
		Entries.reserve(Count);

		for (int32 i = 0; i < Count; ++i)
		{
			const int32 ObjIndex = ObjectIndices[Start + i];
			const float Center = ObjectBounds[ObjIndex].GetCenter()[Axis];
			Entries.push_back({ ObjIndex, Center });
		}

		std::sort(Entries.begin(), Entries.end(),
			[](const FAxisEntry& A, const FAxisEntry& B)
			{
				if (A.Center == B.Center)
				{
					return A.Index < B.Index;
				}
				return A.Center < B.Center;
			});

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
			const float CenterA = Entries[i].Center;
			const float CenterB = Entries[i + 1].Center;

			// Skip split candidates between nearly identical centers.
			if (std::abs(CenterA - CenterB) <= MathUtil::Epsilon)
			{
				continue;
			}

			const int32 LeftCount = i + 1;
			const int32 RightCount = Count - LeftCount;

			const float LeftArea = ComputeSurfaceArea(Prefix[i]);
			const float RightArea = ComputeSurfaceArea(Suffix[i + 1]);

			// ParentArea is constant for all candidates, so omit it in comparison.
			const float Cost =
				LeftArea * static_cast<float>(LeftCount) +
				RightArea * static_cast<float>(RightCount);

			const int32 Balance = std::abs(LeftCount - RightCount);

			// 1) Prefer lower SAH cost.
			// 2) If costs are near-equal, prefer more balanced partitions.
			if (!bFound ||
				Cost < BestCost - MathUtil::Epsilon ||
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
FSplitCriterion FBVH::FindSplitPositionFromBounds(const FAABB& NodeAABB)
{
	// Longest Axis Median Split
	const FVector   Extent{ NodeAABB.Max - NodeAABB.Min };
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

	int32 Axis{ GetAxisValue(Criterion.Axis) };
	int32 Left{ Start };
	int32 Right{ Start + Count - 1 };

	while (Left <= Right)
	{
		const FAABB& LeftBox{ ObjectBounds[ObjectIndices[Left]] };
		const FVector LeftCenter{ LeftBox.GetCenter() };
		const float   LeftValue{ LeftCenter[Axis] };

		if (LeftValue < Criterion.Position)
		{
			++Left;
			continue;
		}

		const FAABB& RightBox{ ObjectBounds[ObjectIndices[Right]] };
		const FVector RightCenter{ RightBox.GetCenter() };
		const float   RightValue{ RightCenter[Axis] };

		if (RightValue >= Criterion.Position)
		{
			--Right;
			continue;
		}

		std::swap(ObjectIndices[Left], ObjectIndices[Right]);
		++Left;
		--Right;
	}

	const int32 Mid{ Left };

	// Prevent degenerate split where all objects end up on one side.
	if (Mid == Start || Mid == Start + Count)
	{
		const int32 ForcedMid{ Start + Count / 2 };

		std::sort(ObjectIndices.begin() + Start, ObjectIndices.begin() + Start + Count,
			[&](int32 A, int32 B)
			{
				const FVector CenterA{ ObjectBounds[A].GetCenter() };
				const FVector CenterB{ ObjectBounds[B].GetCenter() };
				return CenterA[Axis] < CenterB[Axis];
			});
		return ForcedMid;
	}
	return Mid;
}

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
 * @brief Refit one node's bounds from either objects (leaf) or children (internal).
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Node index to refit.
 */
void FBVH::RefitNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
	FNode& Node = Nodes[NodeIndex];

	if (Node.ObjectCount <= 0 || Node.FirstObject == INDEX_NONE)
	{
		return;
	}

	if (Node.IsLeaf())
	{
		Node.Bounds = ComputeBounds(ObjectBounds, ObjectIndices, Node.FirstObject, Node.ObjectCount);
	}
	else
	{
		assert(Node.Left != INDEX_NONE && Node.Right != INDEX_NONE);
		Node.Bounds = UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
	}
}

/**
 * @brief Reconstruct parent links by traversing from the root.
 */
void FBVH::RebuildParentLinks()
{
	for (FNode& Node : Nodes)
	{
		Node.Parent = INDEX_NONE;
	}

	if (RootNodeIndex == INDEX_NONE)
	{
		return;
	}

	TArray<int32> Stack;
	Stack.reserve(Nodes.size());
	Stack.push_back(RootNodeIndex);

	while (!Stack.empty())
	{
		const int32 NodeIndex = Stack.back();
		Stack.pop_back();

		FNode& Node = Nodes[NodeIndex];

		if (Node.Left != INDEX_NONE)
		{
			Nodes[Node.Left].Parent = NodeIndex;
			Stack.push_back(Node.Left);
		}
		if (Node.Right != INDEX_NONE)
		{
			Nodes[Node.Right].Parent = NodeIndex;
			Stack.push_back(Node.Right);
		}
	}
}

/**
 * @brief Try rotating against the left child to lower local SAH cost.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Target internal node index.
 * @return True if a beneficial rotation was applied.
 */
bool FBVH::TryRotateWithLeftChild(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
	FNode& Node = Nodes[NodeIndex];
	if (Node.IsLeaf() || Node.Left == INDEX_NONE || Node.Right == INDEX_NONE)
	{
		return false;
	}

	const int32 AIndex = Node.Left;
	const int32 BIndex = Node.Right;
	FNode& A = Nodes[AIndex];

	if (A.IsLeaf())
	{
		return false;
	}

	const float OldCost = ComputeSurfaceArea(Node.Bounds) + ComputeSurfaceArea(A.Bounds);

	float BestCost = OldCost;
	int32 BestGrandChild = INDEX_NONE;
	bool bUseALeft = false;

	// Candidate 1: swap B with A.Left.
	{
		const int32 G = A.Left;
		const int32 O = A.Right;

		const FAABB NewA = UnionBounds(Nodes[BIndex].Bounds, Nodes[O].Bounds);
		const FAABB NewNode = UnionBounds(NewA, Nodes[G].Bounds);
		const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewA);

		if (NewCost < BestCost)
		{
			BestCost = NewCost;
			BestGrandChild = G;
			bUseALeft = true;
		}
	}

	// Candidate 2: swap B with A.Right.
	{
		const int32 G = A.Right;
		const int32 O = A.Left;

		const FAABB NewA = UnionBounds(Nodes[BIndex].Bounds, Nodes[O].Bounds);
		const FAABB NewNode = UnionBounds(NewA, Nodes[G].Bounds);
		const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewA);

		if (NewCost < BestCost)
		{
			BestCost = NewCost;
			BestGrandChild = G;
			bUseALeft = false;
		}
	}

	if (BestGrandChild == INDEX_NONE)
	{
		return false;
	}

	// Apply chosen rotation.
	if (bUseALeft)
	{
		Node.Right = A.Left;
		A.Left = BIndex;
	}
	else
	{
		Node.Right = A.Right;
		A.Right = BIndex;
	}

	Nodes[Node.Right].Parent = NodeIndex;
	Nodes[BIndex].Parent = AIndex;
	A.Parent = NodeIndex;

	RefitNode(ObjectBounds, AIndex);
	RefitNode(ObjectBounds, NodeIndex);
	return true;
}

/**
 * @brief Try rotating against the right child to lower local SAH cost.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Target internal node index.
 * @return True if a beneficial rotation was applied.
 */
bool FBVH::TryRotateWithRightChild(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
	FNode& Node = Nodes[NodeIndex];
	if (Node.IsLeaf() || Node.Left == INDEX_NONE || Node.Right == INDEX_NONE)
	{
		return false;
	}

	const int32 AIndex = Node.Left;
	const int32 BIndex = Node.Right;
	FNode& B = Nodes[BIndex];

	if (B.IsLeaf())
	{
		return false;
	}

	const float OldCost = ComputeSurfaceArea(Node.Bounds) + ComputeSurfaceArea(B.Bounds);

	float BestCost = OldCost;
	int32 BestGrandChild = INDEX_NONE;
	bool bUseBLeft = false;

	// Candidate 1: swap A with B.Left.
	{
		const int32 G = B.Left;
		const int32 O = B.Right;

		const FAABB NewB = UnionBounds(Nodes[AIndex].Bounds, Nodes[O].Bounds);
		const FAABB NewNode = UnionBounds(Nodes[G].Bounds, NewB);
		const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewB);

		if (NewCost < BestCost)
		{
			BestCost = NewCost;
			BestGrandChild = G;
			bUseBLeft = true;
		}
	}

	// Candidate 2: swap A with B.Right.
	{
		const int32 G = B.Right;
		const int32 O = B.Left;

		const FAABB NewB = UnionBounds(Nodes[AIndex].Bounds, Nodes[O].Bounds);
		const FAABB NewNode = UnionBounds(Nodes[G].Bounds, NewB);
		const float NewCost = ComputeSurfaceArea(NewNode) + ComputeSurfaceArea(NewB);

		if (NewCost < BestCost)
		{
			BestCost = NewCost;
			BestGrandChild = G;
			bUseBLeft = false;
		}
	}

	if (BestGrandChild == INDEX_NONE)
	{
		return false;
	}

	// Apply chosen rotation.
	if (bUseBLeft)
	{
		Node.Left = B.Left;
		B.Left = AIndex;
	}
	else
	{
		Node.Left = B.Right;
		B.Right = AIndex;
	}

	Nodes[Node.Left].Parent = NodeIndex;
	Nodes[AIndex].Parent = BIndex;
	B.Parent = NodeIndex;

	RefitNode(ObjectBounds, BIndex);
	RefitNode(ObjectBounds, NodeIndex);
	return true;
}

/**
 * @brief Try local rotations for a node.
 * @param ObjectBounds Updated per-object AABBs.
 * @param NodeIndex Target node index.
 * @return True if any rotation was applied.
 */
bool FBVH::TryRotateNode(const TArray<FAABB>& ObjectBounds, int32 NodeIndex)
{
	if (Nodes[NodeIndex].IsLeaf())
	{
		return false;
	}

	// Stop at first successful rotation in this pass.
	if (TryRotateWithLeftChild(ObjectBounds, NodeIndex))
	{
		return true;
	}
	if (TryRotateWithRightChild(ObjectBounds, NodeIndex))
	{
		return true;
	}
	return false;
}

/**
 * @brief Validate parent/child topology invariants using assertions.
 */
void FBVH::ValidateBVH() const
{
	assert(RootNodeIndex == INDEX_NONE || Nodes[RootNodeIndex].Parent == INDEX_NONE);

	for (int32 i = 0; i < static_cast<int32>(Nodes.size()); ++i)
	{
		const FBVH::FNode& Node = Nodes[i];

		if (Node.IsLeaf())
		{
			assert(Node.Left == INDEX_NONE && Node.Right == INDEX_NONE);
		}
		else
		{
			assert(Node.Left != INDEX_NONE && Node.Right != INDEX_NONE);
			assert(Nodes[Node.Left].Parent == i);
			assert(Nodes[Node.Right].Parent == i);
		}
	}
}
