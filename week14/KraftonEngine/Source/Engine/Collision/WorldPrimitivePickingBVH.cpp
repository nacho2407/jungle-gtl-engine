#include "Collision/WorldPrimitivePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
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

	// 레이의 broadcast 값과 inverse direction을 한 번만 준비하고,
	// 내부 노드마다 8개 child AABB를 packet 단위로 재사용합니다.
	const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(Ray.Origin, Ray.Direction);

	NodeStack.push_back({ 0, RootTMin });
	while (!NodeStack.empty())
	{
		const FTraversalEntry Entry = NodeStack.back();
		NodeStack.pop_back();
		if (Entry.TMin > OutHitResult.Distance)
		{
			continue;
		}

		const int32 NodeIndex = Entry.NodeIndex;
		const FNode& Node = Nodes[NodeIndex];

		if (Node.IsLeaf())
		{
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

		// Broad phase는 helper에 맡기고, traversal 순서 결정은 BVH가 직접 담당합니다.
		float TMinValues[8];
		const int32 mask = FRayUtilsSIMD::IntersectAABB8(
			RayContext,
			Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
			Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
			OutHitResult.Distance,
			TMinValues);
		if (mask == 0) continue;

		// 충돌한 자식들을 TMin 기준으로 정렬하여 스택에 넣기 위해 수집
		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;

		for (int32 i = 0; i < 8; ++i)
		{
			if ((mask >> i) & 1)
			{
				ChildEntries[ChildEntryCount++] = { Node.Children[i], TMinValues[i] };
			}
		}

		// 거리에 따른 간단한 삽입 정렬 (가까운 것을 나중에 넣어서 먼저 팝되도록 함)
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

		const int32 ChildIdx = BuildRecursive(RangeStart, RangeEnd);
		const int32 LocalChildIdx = Nodes[NodeIndex].ChildCount;
		
		Nodes[NodeIndex].Children[LocalChildIdx] = ChildIdx;
		
		// 자식 bounds를 SOA 형태로 캐시해 raycast 시 gather 없이 바로 SIMD 검사합니다.
		const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
		Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
		Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
		Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
		Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
		Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
		Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

		Nodes[NodeIndex].ChildCount++;
	}

	// 나머지 빈 슬롯(ChildCount ~ 7)은 무효한 박스(Min > Max)로 채워 
	// SIMD 연산 시 자연스럽게 실패하게 만듭니다.
	for (int32 i = Nodes[NodeIndex].ChildCount; i < 8; ++i)
	{
		Nodes[NodeIndex].ChildMinX[i] = 1e30f;
		Nodes[NodeIndex].ChildMinY[i] = 1e30f;
		Nodes[NodeIndex].ChildMinZ[i] = 1e30f;
		Nodes[NodeIndex].ChildMaxX[i] = -1e30f;
		Nodes[NodeIndex].ChildMaxY[i] = -1e30f;
		Nodes[NodeIndex].ChildMaxZ[i] = -1e30f;
	}

	return NodeIndex;
}
