#include "BVH.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Frustum.h"
#include <algorithm>

namespace
{
	FBoxSphereBounds ToSphereBounds(const FAABB& InBounds)
	{
		const FVector Center = (InBounds.PMin + InBounds.PMax) * 0.5f;
		const FVector Extent = (InBounds.PMax - InBounds.PMin) * 0.5f;
		return { Center, Extent.Size(), Extent };
	}
}

BVH::~BVH()
{
	Reset();
}

void BVH::Reset()
{
	DestroyNode(Root);
	Root = nullptr;
	PrimitiveRefs.clear();
}

void BVH::DestroyNode(BuildNode* Node)
{
	if (!Node) return;

	DestroyNode(Node->Left);
	DestroyNode(Node->Right);
	delete Node;
}

void BVH::Build(const TArray<UPrimitiveComponent*>& InPrimitives)
{
	Reset();

	PrimitiveRefs.reserve(InPrimitives.size());
	for (UPrimitiveComponent* Primitive : InPrimitives)
	{
		if (!Primitive || Primitive->IsPendingKill())
		{
			continue;
		}

		const FBoxSphereBounds WorldBounds = Primitive->GetWorldBounds();
		const FVector Min = WorldBounds.Center - WorldBounds.BoxExtent;
		const FVector Max = WorldBounds.Center + WorldBounds.BoxExtent;

		FPrimRef Ref;
		Ref.Bounds = FAABB(Min, Max);
		Ref.Centroid = WorldBounds.Center;
		Ref.Center = WorldBounds.Center;
		Ref.Extent = WorldBounds.BoxExtent;
		Ref.Primitive = Primitive;
		PrimitiveRefs.push_back(Ref);
	}

	if (PrimitiveRefs.empty())
	{
		return;
	}

	Root = BuildRecursive(0, static_cast<int32>(PrimitiveRefs.size()));
}

BuildNode* BVH::BuildRecursive(int32 Start, int32 End, int32 Depth)
{
	BuildNode* Node = new BuildNode();
	const int32 Count = End - Start;

	FAABB NodeBounds;
	FAABB CentroidBounds;
	for (int32 Index = Start; Index < End; ++Index)
	{
		NodeBounds.Expand(PrimitiveRefs[Index].Bounds);
		CentroidBounds.Expand(PrimitiveRefs[Index].Centroid);
	}

	Node->Bounds = NodeBounds;
	Node->Center = NodeBounds.Centroid();
	Node->Extent = (NodeBounds.PMax - NodeBounds.PMin) * 0.5f;
	Node->SubtreePrimCount = Count;

	if (Count <= MaxPrimitivesPerLeaf || Depth >= MaxDepth)
	{
		Node->FirstPrimOffset = Start;
		Node->PrimCount = Count;
		return Node;
	}

	const int32 Axis = CentroidBounds.MaxExtentAxis();
	const float CentroidMin = GetAxis(CentroidBounds.PMin, Axis);
	const float CentroidMax = GetAxis(CentroidBounds.PMax, Axis);

	if (std::fabs(CentroidMax - CentroidMin) < 1e-5f)
	{
		Node->FirstPrimOffset = Start;
		Node->PrimCount = Count;
		return Node;
	}


	FBucket Buckets[NUM_BUCKETS] = {};
	for (int32 i = Start; i < End; ++i)
	{
		float t = (GetAxis(PrimitiveRefs[i].Centroid, Axis) - CentroidMin) / (CentroidMax - CentroidMin);
		int b = std::clamp((int32)(t * NUM_BUCKETS), 0, NUM_BUCKETS - 1);
		Buckets[b].Count++;
		Buckets[b].Bounds.Expand(PrimitiveRefs[i].Bounds);
	}

	// Prefix sweep: PrefixBounds[i] = buckets [0..i] 합산
	FAABB PrefixBounds[NUM_BUCKETS];
	int32 PrefixCount[NUM_BUCKETS];
	PrefixBounds[0] = Buckets[0].Bounds;
	PrefixCount[0]  = Buckets[0].Count;
	for (int32 i = 1; i < NUM_BUCKETS; ++i)
	{
		PrefixBounds[i] = PrefixBounds[i - 1];
		PrefixBounds[i].Expand(Buckets[i].Bounds);
		PrefixCount[i] = PrefixCount[i - 1] + Buckets[i].Count;
	}

	// Suffix sweep: SuffixBounds[i] = buckets [i..NUM_BUCKETS-1] 합산
	FAABB SuffixBounds[NUM_BUCKETS];
	int32 SuffixCount[NUM_BUCKETS];
	SuffixBounds[NUM_BUCKETS - 1] = Buckets[NUM_BUCKETS - 1].Bounds;
	SuffixCount[NUM_BUCKETS - 1]  = Buckets[NUM_BUCKETS - 1].Count;
	for (int32 i = NUM_BUCKETS - 2; i >= 0; --i)
	{
		SuffixBounds[i] = SuffixBounds[i + 1];
		SuffixBounds[i].Expand(Buckets[i].Bounds);
		SuffixCount[i] = SuffixCount[i + 1] + Buckets[i].Count;
	}

	// split i: 왼쪽 [0..i-1], 오른쪽 [i..NUM_BUCKETS-1]
	float BestCost = std::numeric_limits<float>::max();
	int32 BestSplit = -1;
	const float ParentArea = NodeBounds.SurfaceArea();

	for (int32 i = 1; i < NUM_BUCKETS; ++i)
	{
		float Cost = 1.0f;
		if (ParentArea > 1e-8f)
		{
			Cost += (PrefixBounds[i - 1].SurfaceArea() * PrefixCount[i - 1]
				   + SuffixBounds[i].SurfaceArea() * SuffixCount[i]) / ParentArea;
		}

		if (Cost < BestCost) { BestCost = Cost; BestSplit = i; }
	}

	if (BestSplit < 0)
	{
		Node->FirstPrimOffset = Start;
		Node->PrimCount = Count;
		return Node;
	}

	float SplitPos = CentroidMin + (CentroidMax - CentroidMin) * ((float)BestSplit / NUM_BUCKETS);

	auto MidIt = std::partition(
		PrimitiveRefs.begin() + Start,
		PrimitiveRefs.begin() + End,
		[Axis, SplitPos](const FPrimRef& P)
		{
			return GetAxis(P.Centroid, Axis) < SplitPos;
		}
	);


	int32 Mid = (int32)(MidIt - PrimitiveRefs.begin());

	if (Mid == Start || Mid == End)
	{
		Mid = Start + Count / 2;
		std::nth_element(
			PrimitiveRefs.begin() + Start,
			PrimitiveRefs.begin() + Mid,
			PrimitiveRefs.begin() + End,
			[Axis](const FPrimRef& A, const FPrimRef& B)
			{
				return GetAxis(A.Centroid, Axis) < GetAxis(B.Centroid, Axis);
			});
	}


	Node->SplitAxis = Axis;
	const int32 ChildDepth = Depth + 1;
	Node->Left = BuildRecursive(Start, Mid, ChildDepth);
	Node->Right = BuildRecursive(Mid, End, ChildDepth);
	Node->SubtreePrimCount =
		(Node->Left ? Node->Left->SubtreePrimCount : 0) +
		(Node->Right ? Node->Right->SubtreePrimCount : 0);
	return Node;
}

void BVH::QueryFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!Root)
	{
		return;
	}

	constexpr int32 MaxTraversalDepth = 64;
	const BuildNode* NodeStack[MaxTraversalDepth];
	int32 StackSize = 0;
	NodeStack[StackSize++] = Root;

	while (StackSize > 0)
	{
		const BuildNode* Node = NodeStack[--StackSize];
		if (!Node)
		{
			continue;
		}

		const FFrustum::EContainment NodeContainment = Frustum.TestAABB(Node->Center, Node->Extent);
		if (NodeContainment == FFrustum::EContainment::Outside)
		{
			continue;
		}

		if (NodeContainment == FFrustum::EContainment::Inside)
		{
			AppendNodePrimitives(Node, OutPrimitives);
			continue;
		}

		if (Node->IsLeaf())
		{
			for (int32 Index = 0; Index < Node->PrimCount; ++Index)
			{
				const FPrimRef& Ref = PrimitiveRefs[Node->FirstPrimOffset + Index];
				if (Ref.Primitive && Frustum.TestAABB(Ref.Center, Ref.Extent) != FFrustum::EContainment::Outside)
				{
					OutPrimitives.push_back(Ref.Primitive);
				}
			}
			continue;
		}

		if (Node->Right)
		{
			NodeStack[StackSize++] = Node->Right;
		}
		if (Node->Left)
		{
			NodeStack[StackSize++] = Node->Left;
		}
	}
}

void BVH::QueryFrustumRecursive(const BuildNode* Node, const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!Node)
	{
		return;
	}

	const FVector NodeCenter = Node->Bounds.Centroid();
	const FVector NodeExtent = (Node->Bounds.PMax - Node->Bounds.PMin) * 0.5f;
	const FFrustum::EContainment NodeContainment = Frustum.TestAABB(NodeCenter, NodeExtent);
	if (NodeContainment == FFrustum::EContainment::Outside)
	{
		return;
	}

	if (NodeContainment == FFrustum::EContainment::Inside)
	{
		AppendNodePrimitives(Node, OutPrimitives);
		return;
	}

	if (Node->IsLeaf())
	{
		for (int32 Index = 0; Index < Node->PrimCount; ++Index)
		{
			const FPrimRef& Ref = PrimitiveRefs[Node->FirstPrimOffset + Index];
			const FVector PrimitiveCenter = Ref.Bounds.Centroid();
			const FVector PrimitiveExtent = (Ref.Bounds.PMax - Ref.Bounds.PMin) * 0.5f;
			if (Ref.Primitive && Frustum.TestAABB(PrimitiveCenter, PrimitiveExtent) != FFrustum::EContainment::Outside)
			{
				OutPrimitives.push_back(Ref.Primitive);
			}
		}
		return;
	}

	QueryFrustumRecursive(Node->Left, Frustum, OutPrimitives);
	QueryFrustumRecursive(Node->Right, Frustum, OutPrimitives);
}

void BVH::AppendNodePrimitives(const BuildNode* Node, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!Node)
	{
		return;
	}

	OutPrimitives.reserve(OutPrimitives.size() + Node->SubtreePrimCount);

	constexpr int32 MaxTraversalDepth = 64;
	const BuildNode* NodeStack[MaxTraversalDepth];
	int32 StackSize = 0;
	NodeStack[StackSize++] = Node;

	while (StackSize > 0)
	{
		const BuildNode* CurrentNode = NodeStack[--StackSize];
		if (!CurrentNode)
		{
			continue;
		}

		if (CurrentNode->IsLeaf())
		{
			for (int32 Index = 0; Index < CurrentNode->PrimCount; ++Index)
			{
				const FPrimRef& Ref = PrimitiveRefs[CurrentNode->FirstPrimOffset + Index];
				if (Ref.Primitive)
				{
					OutPrimitives.push_back(Ref.Primitive);
				}
			}
			continue;
		}

		if (CurrentNode->Right)
		{
			NodeStack[StackSize++] = CurrentNode->Right;
		}
		if (CurrentNode->Left)
		{
			NodeStack[StackSize++] = CurrentNode->Left;
		}
	}
}

void BVH::QueryRay(const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	float TraversalMaxDistance = MaxDistance;
	VisitRay(
		InRay,
		TraversalMaxDistance,
		[&OutPrimitives](UPrimitiveComponent* Primitive, float PrimitiveTNear, float PrimitiveTFar, float& InOutMaxDistance)
		{
			(void)PrimitiveTNear;
			(void)PrimitiveTFar;
			(void)InOutMaxDistance;
			if (Primitive)
			{
				OutPrimitives.push_back(Primitive);
			}
		});
}

void BVH::VisitRay(const Ray& InRay, float& InOutMaxDistance, const FRayHitVisitor& Visitor) const
{
	if (!Root)
	{
		return;
	}

	float RootNear = 0.0f;
	float RootFar = 0.0f;
	if (!Root->Bounds.Intersect(InRay, InOutMaxDistance, RootNear, RootFar))
	{
		return;
	}

	VisitRayRecursive(Root, InRay, InOutMaxDistance, Visitor);
}

void BVH::VisitRayRecursive(const BuildNode* Node, const Ray& InRay, float& InOutMaxDistance, const FRayHitVisitor& Visitor) const
{
	if (!Node)
	{
		return;
	}

	float NodeNear = 0.0f;
	float NodeFar = 0.0f;
	if (!Node->Bounds.Intersect(InRay, InOutMaxDistance, NodeNear, NodeFar))
	{
		return;
	}

	if (Node->IsLeaf())
	{
		for (int32 Index = 0; Index < Node->PrimCount; ++Index)
		{
			const FPrimRef& Ref = PrimitiveRefs[Node->FirstPrimOffset + Index];
			float PrimitiveNear = 0.0f;
			float PrimitiveFar = 0.0f;
			if (Ref.Primitive && Ref.Bounds.Intersect(InRay, InOutMaxDistance, PrimitiveNear, PrimitiveFar))
			{
				Visitor(Ref.Primitive, PrimitiveNear, PrimitiveFar, InOutMaxDistance);
			}
		}
		return;
	}

	float LeftNear = 0.0f;
	float LeftFar = 0.0f;
	const bool bHitLeft = Node->Left && Node->Left->Bounds.Intersect(InRay, InOutMaxDistance, LeftNear, LeftFar);

	float RightNear = 0.0f;
	float RightFar = 0.0f;
	const bool bHitRight = Node->Right && Node->Right->Bounds.Intersect(InRay, InOutMaxDistance, RightNear, RightFar);

	if (bHitLeft && bHitRight)
	{
		const BuildNode* FirstNode = Node->Left;
		const BuildNode* SecondNode = Node->Right;
		float SecondNear = RightNear;

		if (RightNear < LeftNear)
		{
			FirstNode = Node->Right;
			SecondNode = Node->Left;
			SecondNear = LeftNear;
		}

		VisitRayRecursive(FirstNode, InRay, InOutMaxDistance, Visitor);
		if (SecondNear <= InOutMaxDistance)
		{
			VisitRayRecursive(SecondNode, InRay, InOutMaxDistance, Visitor);
		}
	}
	else if (bHitLeft)
	{
		VisitRayRecursive(Node->Left, InRay, InOutMaxDistance, Visitor);
	}
	else if (bHitRight)
	{
		VisitRayRecursive(Node->Right, InRay, InOutMaxDistance, Visitor);
	}
}
