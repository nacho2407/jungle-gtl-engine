#include "Collision/WorldPrimitivePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"

#include <algorithm>

/**
 * 월드 내 actor/primitive의 배치가 바뀌었음을 표시해 다음 raycast 전에 BVH를 다시 빌드하게 합니다.
 */ 
void FWorldPrimitivePickingBVH::MarkDirty()
{
	bDirty = true;
}

/**
 * 현재 보이는 primitive 중 실제로 picking 가능한 대상만 leaf로 구성하며 트리를 빌드합니다.
 * 
 * \param Actors
 */
void FWorldPrimitivePickingBVH::BuildNow(const TArray<AActor*>& Actors)
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

/**
 * 월드 상태가 변경되어 dirty로 표시된 경우에만 BVH를 다시 빌드합니다.
 *
 * \param Actors 현재 월드 actor 목록
 */
void FWorldPrimitivePickingBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
	if (!bDirty)
	{
		return;
	}

	BuildNow(Actors);
}

/**
 * 월드 공간 ray로 오브젝트 BVH를 순회하면서 가장 가까운 primitive hit를 찾습니다.
 * broad phase에서는 노드 AABB만 검사하고, leaf에 도달한 primitive만 component 수준 line trace를 수행합니다.
 *
 * \param Ray 월드 공간 ray
 * \param OutHitResult 가장 가까운 hit 결과
 * \param OutActor hit된 primitive의 owner actor
 * \return 유효한 actor를 하나라도 맞췄으면 true
 */
bool FWorldPrimitivePickingBVH::Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	struct FTraversalEntry
	{
		int32 NodeIndex;
		float TMin;
	};

	OutHitResult = {};
	OutActor = nullptr;
	if (Nodes.empty())
	{
		return false;
	}

	TArray<FTraversalEntry> NodeStack;
	NodeStack.reserve(64);

	float RootTMin = 0.0f;
	float RootTMax = 0.0f;
	if (!FRayUtils::IntersectRayAABB(Ray, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootTMin, RootTMax))
	{
		return false;
	}

	NodeStack.push_back({ 0, RootTMin });
	while (!NodeStack.empty())
	{
		const FTraversalEntry Entry = NodeStack.back();
		NodeStack.pop_back();
		if (Entry.TMin > OutHitResult.Distance)
		{
			continue;
		}

		const int32 NodeIndex = Entry.NodeIndex; //이번 노드
		//이번 노드의 BVH AABB와 충돌하지 않으면 스킵.
		const FNode& Node = Nodes[NodeIndex];
		//리프 노드라면 실제 primitive와 충돌 검사 수행
		if (Node.IsLeaf())
		{
			//leaf 노드는 연속된 primitive 구간을 보관하며 트리 깊이를 너무 늘어나는 걸 차단합니다
			for (int32 LeafIndex = Node.FirstLeaf; LeafIndex < Node.FirstLeaf + Node.LeafCount; ++LeafIndex)
			{
				const FLeaf& Leaf = Leaves[LeafIndex];
				FHitResult CandidateHit{};
				if (Leaf.Primitive->LineTraceComponent(Ray, CandidateHit) &&
					CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
					OutActor = Leaf.Owner;
				}
			}
			continue;
		}

		// 내부 노드는 최대 8개의 자식을 가지므로 AABB에 걸린 자식들을 계속 순회합니다.
		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;
		for (int32 ChildIndex = 0; ChildIndex < Node.ChildCount; ++ChildIndex)
		{
			const int32 ChildNodeIndex = Node.Children[ChildIndex];
			if (ChildNodeIndex < 0)
			{
				continue;
			}

			float ChildTMin = 0.0f;
			float ChildTMax = 0.0f;
			const FNode& ChildNode = Nodes[ChildNodeIndex];
			if (!FRayUtils::IntersectRayAABB(Ray, ChildNode.Bounds.Min, ChildNode.Bounds.Max, ChildTMin, ChildTMax))
			{
				continue;
			}
			if (ChildTMin > OutHitResult.Distance)
			{
				continue;
			}

			ChildEntries[ChildEntryCount++] = { ChildNodeIndex, ChildTMin };
		}

		for (int32 I = 1; I < ChildEntryCount; ++I)
		{
			FTraversalEntry Key = ChildEntries[I];
			int32 J = I - 1;
			while (J >= 0 && ChildEntries[J].TMin < Key.TMin)
			{
				ChildEntries[J + 1] = ChildEntries[J];
				--J;
			}
			ChildEntries[J + 1] = Key;
		}

		for (int32 I = 0; I < ChildEntryCount; ++I)
		{
			NodeStack.push_back(ChildEntries[I]);
		}
	}
	return OutActor != nullptr;
}

/**
 * [Start, End) 구간의 primitive leaf들로부터 BVH 노드를 재귀적으로 생성합니다.
 * leaf 수가 작으면 종료하고, 많으면 centroid 분포가 가장 큰 축을 따라 최대 8개 구간으로 분할합니다.
 *
 * \param Start leaf 구간 시작 인덱스
 * \param End leaf 구간 끝 다음 인덱스
 * \return 생성된 노드 인덱스
 */
int32 FWorldPrimitivePickingBVH::BuildRecursive(int32 Start, int32 End)
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

		// centroid가 가장 넓게 퍼진 축을 고르고, 그 축 기준으로 정렬한 뒤 8-way 분할한다.
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

	std::sort(
		Leaves.begin() + Start,
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

	const int32 DesiredChildren = std::min<int32>(8, LeafCount);
	for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
	{
		const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
		const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
		if (RangeStart >= RangeEnd)
		{
			continue;
		}

		Nodes[NodeIndex].Children[Nodes[NodeIndex].ChildCount++] = BuildRecursive(RangeStart, RangeEnd);
	}

	return NodeIndex;
}
