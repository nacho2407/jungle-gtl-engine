#include "Collision/PickingBVH.h"

#include "Collision/RayUtils.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"

#include <algorithm>

void FPickingBVH::MarkDirty()
{
	bDirty = true;
}

/**
 * 현재 보이는 primitive 중 실제로 픽킹 가능한 대상만 leaf로 구성하며 트리를 빌드합니다.
 * 
 * \param Actors
 */
void FPickingBVH::BuildNow(const TArray<AActor*>& Actors)
{
	Leaves.clear();
	Nodes.clear();

	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible())
			{
				continue;
			}

			FLeaf Leaf;
			Leaf.Primitive = Primitive;
			Leaf.Owner = Actor;
			Leaf.Bounds = Primitive->GetWorldBoundingBox();

			if (!Leaf.Bounds.IsValid())
			{
				continue;
			}

			Leaves.push_back(Leaf);
		}
	}

	if (!Leaves.empty())
	{
		BuildRecursive(0, static_cast<int32>(Leaves.size()));
	}

	bDirty = false;
}

void FPickingBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
	if (!bDirty)
	{
		return;
	}

	BuildNow(Actors);
}

bool FPickingBVH::Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	OutHitResult = {};
	OutActor = nullptr;
	if (Nodes.empty())
	{
		return false;
	}

	TArray<int32> NodeStack;
	NodeStack.push_back(0);

	while (!NodeStack.empty())
	{
		const int32 NodeIndex = NodeStack.back();
		NodeStack.pop_back();

		const FNode& Node = Nodes[NodeIndex];
		if (!FRayUtils::CheckRayAABB(Ray, Node.Bounds.Min, Node.Bounds.Max))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			//leaf 노드는 연속된 primitive 구간을 보관하며 트리 깊이를 너무 늘어나는 걸 차단합니다
			for (int32 LeafIndex = Node.FirstLeaf; LeafIndex < Node.FirstLeaf + Node.LeafCount; ++LeafIndex)
			{
				const FLeaf& Leaf = Leaves[LeafIndex];
				if (!Leaf.Primitive || !Leaf.Owner || !Leaf.Owner->IsVisible() || !Leaf.Primitive->IsVisible())
				{
					continue;
				}

				FHitResult CandidateHit{};
				if (FRayUtils::RaycastComponent(Leaf.Primitive, Ray, CandidateHit) &&
					CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
					OutActor = Leaf.Owner;
				}
			}
			continue;
		}

		if (Node.LeftChild >= 0)
		{
			NodeStack.push_back(Node.LeftChild);
		}
		if (Node.RightChild >= 0)
		{
			NodeStack.push_back(Node.RightChild);
		}
	}

	return OutActor != nullptr;
}

/**
 * 재귀적으로 BVH 트리를 빌드하는 함수입니다. Start부터 End까지의 leaf들을 포함하는 노드를 만들고,
 * leaf 수가 MaxLeafSize보다 크면 centroid 기준으로 분할하여 자식 노드를 만듭니다.
 * 
 * \param Start
 * \param End
 * \return 
 */
int32 FPickingBVH::BuildRecursive(int32 Start, int32 End)
{
	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();

	FBoundingBox Bounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		const FBoundingBox& LeafBounds = Leaves[LeafIndex].Bounds;
		Bounds.Expand(LeafBounds.Min);
		Bounds.Expand(LeafBounds.Max);
	}
	Nodes[NodeIndex].Bounds = Bounds;

	constexpr int32 MaxLeafSize = 8;
	const int32 LeafCount = End - Start;
	if (LeafCount <= MaxLeafSize)
	{
		Nodes[NodeIndex].FirstLeaf = Start;
		Nodes[NodeIndex].LeafCount = LeafCount;
		return NodeIndex;
	}

	// centroid가 가장 넓게 퍼진 축을 고르고, 그 축 기준 median으로 분할한다.
	FBoundingBox CentroidBounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		CentroidBounds.Expand(Leaves[LeafIndex].Bounds.GetCenter());
	}

	const FVector CentroidExtent = CentroidBounds.GetExtent();
	int32 SplitAxis = 0;
	if (CentroidExtent.Y > CentroidExtent.X && CentroidExtent.Y >= CentroidExtent.Z)
	{
		SplitAxis = 1;
	}
	else if (CentroidExtent.Z > CentroidExtent.X && CentroidExtent.Z > CentroidExtent.Y)
	{
		SplitAxis = 2;
	}

	const int32 Mid = Start + LeafCount / 2;
	std::nth_element(
		Leaves.begin() + Start,
		Leaves.begin() + Mid,
		Leaves.begin() + End,
		[SplitAxis](const FLeaf& A, const FLeaf& B)
		{
			const FVector CenterA = A.Bounds.GetCenter();
			const FVector CenterB = B.Bounds.GetCenter();
			if (SplitAxis == 1)
			{
				return CenterA.Y < CenterB.Y;
			}
			if (SplitAxis == 2)
			{
				return CenterA.Z < CenterB.Z;
			}
			return CenterA.X < CenterB.X;
		});

	Nodes[NodeIndex].LeftChild = BuildRecursive(Start, Mid);
	Nodes[NodeIndex].RightChild = BuildRecursive(Mid, End);
	return NodeIndex;
}
