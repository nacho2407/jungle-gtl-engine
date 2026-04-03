#include "BVH.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Frustum.h"
#include <algorithm>

namespace
{
	FBoxSphereBounds ToSphereBounds(const AABB& InBounds)
	{
		const FVector Center = (InBounds.pMin + InBounds.pMax) * 0.5f;
		const FVector Extent = (InBounds.pMax - InBounds.pMin) * 0.5f;
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

	DestroyNode(Node->left);
	DestroyNode(Node->right);
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

		PrimRef Ref;
		Ref.bounds = AABB(Min, Max);
		Ref.centroid = Ref.bounds.centroid();
		Ref.primitive = Primitive;
		PrimitiveRefs.push_back(Ref);
	}

	if (PrimitiveRefs.empty())
	{
		return;
	}

	Root = BuildRecursive(0, static_cast<int32>(PrimitiveRefs.size()));
}

BuildNode* BVH::BuildRecursive(int32 Start, int32 End)
{
	BuildNode* Node = new BuildNode();
	const int32 Count = End - Start;

	AABB NodeBounds;
	AABB CentroidBounds;
	for (int32 Index = Start; Index < End; ++Index)
	{
		NodeBounds.expand(PrimitiveRefs[Index].bounds);
		CentroidBounds.expand(PrimitiveRefs[Index].centroid);
	}

	Node->bounds = NodeBounds;

	if (Count <= MaxPrimitivesPerLeaf)
	{
		Node->firstPrimOffset = Start;
		Node->primCount = Count;
		return Node;
	}

	const int32 Axis = CentroidBounds.maxExtentAxis();
	const float CentroidMin = GetAxis(CentroidBounds.pMin, Axis);
	const float CentroidMax = GetAxis(CentroidBounds.pMax, Axis);

	if (std::fabs(CentroidMax - CentroidMin) < 1e-5f)
	{
		Node->firstPrimOffset = Start;
		Node->primCount = Count;
		return Node;
	}


	TArray<FBucket> Buckets(NUM_BUCKETS);

	for (int32 i = Start; i < End; ++i)
	{
		float t = (GetAxis(PrimitiveRefs[i].centroid, Axis) - CentroidMin) / (CentroidMax - CentroidMin);
		int b = std::clamp((int32)(t * NUM_BUCKETS), 0, NUM_BUCKETS - 1);
		Buckets[b].Count++;
		Buckets[b].Bounds.expand(PrimitiveRefs[i].bounds);
	}

	float BestCost = std::numeric_limits<float>::max();
	int32 BestSplit = -1;

	for (int32 i = 1; i < NUM_BUCKETS; ++i)
	{
		AABB L, R;
		int32 NL = 0, NR = 0;
		for (int32 j = 0; j < i; ++j) { L.expand(Buckets[j].Bounds); NL += Buckets[j].Count; }
		for (int32 j = i; j < NUM_BUCKETS; ++j) { R.expand(Buckets[j].Bounds); NR += Buckets[j].Count; }

		float Cost = 1.0f + (L.surfaceArea() * NL + R.surfaceArea() * NR) / NodeBounds.surfaceArea();
		if (Cost < BestCost) { BestCost = Cost; BestSplit = i; }
	}

	float SplitPos = CentroidMin + (CentroidMax - CentroidMin) * ((float)BestSplit / NUM_BUCKETS);

	auto MidIt = std::partition(
		PrimitiveRefs.begin() + Start,
		PrimitiveRefs.begin() + End,
		[Axis, SplitPos](const PrimRef& P)
		{
			return GetAxis(P.centroid, Axis) < SplitPos;
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
			[Axis](const PrimRef& A, const PrimRef& B)
			{
				return GetAxis(A.centroid, Axis) < GetAxis(B.centroid, Axis);
			});
	}


	Node->splitAxis = Axis;
	Node->left = BuildRecursive(Start, Mid);
	Node->right = BuildRecursive(Mid, End);
	return Node;
}

void BVH::QueryFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	QueryFrustumRecursive(Root, Frustum, OutPrimitives);
}

void BVH::QueryFrustumRecursive(const BuildNode* Node, const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!Node)
	{
		return;
	}

	if (!Frustum.IsVisible(ToSphereBounds(Node->bounds)))
	{
		return;
	}

	if (Node->isLeaf())
	{
		for (int32 Index = 0; Index < Node->primCount; ++Index)
		{
			const PrimRef& Ref = PrimitiveRefs[Node->firstPrimOffset + Index];
			if (Ref.primitive && Frustum.IsVisible(ToSphereBounds(Ref.bounds)))
			{
				OutPrimitives.push_back(Ref.primitive);
			}
		}
		return;
	}

	QueryFrustumRecursive(Node->left, Frustum, OutPrimitives);
	QueryFrustumRecursive(Node->right, Frustum, OutPrimitives);
}

void BVH::QueryRay(const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	QueryRayRecursive(Root, InRay, MaxDistance, OutPrimitives);
}

void BVH::QueryRayRecursive(const BuildNode* Node, const Ray& InRay, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	if (!Node || !Node->bounds.intersect(InRay, MaxDistance))
	{
		return;
	}

	if (Node->isLeaf())
	{
		for (int32 Index = 0; Index < Node->primCount; ++Index)
		{
			const PrimRef& Ref = PrimitiveRefs[Node->firstPrimOffset + Index];
			if (Ref.primitive)
			{
				OutPrimitives.push_back(Ref.primitive);
			}
		}
		return;
	}

	QueryRayRecursive(Node->left, InRay, MaxDistance, OutPrimitives);
	QueryRayRecursive(Node->right, InRay, MaxDistance, OutPrimitives);
}
