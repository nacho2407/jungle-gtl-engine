#include "Collision/WorldPrimitivePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Profiling/PlatformTime.h"
#include "GameFramework/AActor.h"

#include <algorithm>

/**
 * BVH 트리를 설정합니다.
 */
namespace
{
	constexpr int32 WorldBVHChildFanout = 8;		//각 노드의 최대 자식 수 (8이면 SIMD와 호응)
	constexpr int32 WorldBVHLeafPacketSize = 8;		//각 리프 노드의 최대 프리미티브 수 (8이면 SIMD와 호응)
	constexpr int32 WorldBVHMaxLeafSize = 16;		//Threshold, 이 기준보다 작으면 더 이상 분할하지 않고 리프로 만듭니다. 
	constexpr int32 WorldBVHMaxTraversalStack = 258;
}

void FWorldPrimitivePickingBVH::MarkDirty()
{
	bDirty = true;
}

void FWorldPrimitivePickingBVH::BuildNow(const TArray<AActor*>& Actors)
{
	Leaves.clear();
	Nodes.clear();
	PrimitivePackets.clear();

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
		PrimitivePackets.reserve((static_cast<int32>(Leaves.size()) + WorldBVHLeafPacketSize - 1) / WorldBVHLeafPacketSize);
		BuildRecursive(0, static_cast<int32>(Leaves.size()));
	}

	bDirty = false;
}

void FWorldPrimitivePickingBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
	if (!bDirty)
	{
		return;
	}

	BuildNow(Actors);
}

/**
 * @brief BVH 트리를 순회하며 주어진 Ray와 교차하는 가장 가까운 프리미티브를 찾습니다.
 * SIMD를 활용하여 (BVH 트리에서 자식으로 이동하며 만나는) 여러 AABB와의 교차 검사를 조금
 * 더 빠르게 수행하며, 거리에 따라 정렬하여 가장 가까운 노드부터 탐색(Front-to-back)하여
 * 불필요한 연산을 줄입니다.
 *
 * @param Ray 쏠 광선(Ray)의 원점과 방향 정보
 * @param OutHitResult 가장 가까운 교차점의 물리적 충돌 결과
 * @param OutActor 교차된 프리미티브를 소유한 액터 포인터
 * @return 교차한 액터가 있으면 true, 없으면 false를 반환합니다.
 */
bool FWorldPrimitivePickingBVH::Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	struct FTraversalEntry
	{
		int32 NodeIndex = -1;
		float TMin = 0.0f;
	};

	OutHitResult = {};
	OutActor = nullptr;
	LastTraversalMetrics = {};

	if (Nodes.empty())
	{
		return false;
	}

	float RootTMin = 0.0f;
	float RootTMax = 0.0f;

	//루트 노드에 대해 AABB 검사 후 실패했다면, picking될 가능성이 없을 테니 바로 return.
	if (!FRayUtils::IntersectRayAABB(Ray, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootTMin, RootTMax))
	{
		return false;
	}

	//SIMD 최적화를 위해 Ray 정보를 미리 SIMD 레지스터에 적재해둡니다. Gather 오버헤드를 줄일 수 있습니다.
	const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(Ray.Origin, Ray.Direction);

	//BVH 트리 순회.
	FTraversalEntry NodeStack[WorldBVHMaxTraversalStack];
	int32 StackSize = 0;
	NodeStack[StackSize++] = { 0, RootTMin };

	while (StackSize > 0)
	{
		const FTraversalEntry Entry = NodeStack[--StackSize];
		if (Entry.TMin > OutHitResult.Distance)
		{
			continue;
		}

		const FNode& Node = Nodes[Entry.NodeIndex];
		if (Node.IsLeaf())
		{
			LastTraversalMetrics.LeafNodesVisited++;

			FTraversalEntry PrimitiveEntries[WorldBVHMaxLeafSize];
			int32 PrimitiveEntryCount = 0;

			for (int32 PacketIndex = 0; PacketIndex < Node.PrimitivePacketCount; ++PacketIndex)
			{
				const FPrimitivePacket& Packet = PrimitivePackets[Node.FirstPrimitivePacket + PacketIndex];
				float PrimitiveTMinValues[8];
				const int32 PrimitiveMask = FRayUtilsSIMD::IntersectAABB8(
					RayContext,
					Packet.MinX, Packet.MinY, Packet.MinZ,
					Packet.MaxX, Packet.MaxY, Packet.MaxZ,
					OutHitResult.Distance,
					PrimitiveTMinValues);
				LastTraversalMetrics.PrimitiveAABBTests += static_cast<uint32>(Packet.PrimitiveCount);

				if (PrimitiveMask == 0)
				{
					continue;
				}

				for (int32 i = 0; i < Packet.PrimitiveCount; ++i)
				{
					if ((PrimitiveMask >> i) & 1)
					{
						LastTraversalMetrics.PrimitiveAABBHits++;
						PrimitiveEntries[PrimitiveEntryCount++] = { Packet.PrimitiveIndices[i], PrimitiveTMinValues[i] };
					}
				}
			}

			int32 ClosestEntryIndex = 0;
			for (int32 I = 1; I < PrimitiveEntryCount; ++I)
			{
				if (PrimitiveEntries[I].TMin < PrimitiveEntries[ClosestEntryIndex].TMin)
				{
					ClosestEntryIndex = I;
				}
			}
			if (PrimitiveEntryCount > 1 && ClosestEntryIndex != 0)
			{
				std::swap(PrimitiveEntries[0], PrimitiveEntries[ClosestEntryIndex]);
			}

			for (int32 EntryIndex = 0; EntryIndex < PrimitiveEntryCount; ++EntryIndex)
			{
				if (PrimitiveEntries[EntryIndex].TMin >= OutHitResult.Distance)
				{
					continue;
				}

				const FLeaf& Leaf = Leaves[PrimitiveEntries[EntryIndex].NodeIndex];
				UPrimitiveComponent* const Primitive = Leaf.Primitive;
				FHitResult CandidateHit{};
				const uint64 NarrowPhaseStart = FPlatformTime::Cycles64();
				LastTraversalMetrics.NarrowPhaseCalls++;

				bool bHit = false;
				if (UStaticMeshComponent* const StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive))
				{
					const FMatrix& WorldMatrix = StaticMeshComponent->GetWorldMatrix();
					const FMatrix& WorldInverse = StaticMeshComponent->GetWorldInverseMatrix();
					bHit = StaticMeshComponent->LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, CandidateHit);
				}
				else
				{
					bHit = Primitive->LineTraceComponent(Ray, CandidateHit);
				}

				if (bHit &&
					CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
					OutActor = Leaf.Owner;
				}

				LastTraversalMetrics.NarrowPhaseMs +=
					FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64() - NarrowPhaseStart);

				const FPrimitivePickingMetrics& PrimitiveMetrics = Primitive->GetLastPickingMetrics();
				LastTraversalMetrics.MeshInternalNodesVisited += PrimitiveMetrics.MeshInternalNodesVisited;
				LastTraversalMetrics.MeshLeafPacketsTested += PrimitiveMetrics.MeshLeafPacketsTested;
				LastTraversalMetrics.MeshTriangleLanesTested += PrimitiveMetrics.MeshTriangleLanesTested;
				LastTraversalMetrics.MeshTraversalMs += PrimitiveMetrics.MeshTraversalMs;
			}
			continue;
		}

		LastTraversalMetrics.InternalNodesVisited++;

		float TMinValues[8];
		const int32 Mask = FRayUtilsSIMD::IntersectAABB8(
			RayContext,
			Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
			Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
			OutHitResult.Distance,
			TMinValues);
		if (Mask == 0)
		{
			continue;
		}

		FTraversalEntry ChildEntries[WorldBVHChildFanout];
		int32 ChildEntryCount = 0;

		for (int32 i = 0; i < Node.ChildCount; ++i)
		{
			if ((Mask >> i) & 1)
			{
				ChildEntries[ChildEntryCount++] = { Node.Children[i], TMinValues[i] };
			}
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

		for (int32 I = 0; I < ChildEntryCount && StackSize < WorldBVHMaxTraversalStack; ++I)
		{
			NodeStack[StackSize++] = ChildEntries[I];
		}
	}

	return OutActor != nullptr;
}

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

	const int32 LeafCount = End - Start;
	if (LeafCount <= WorldBVHMaxLeafSize)
	{
		Nodes[NodeIndex].FirstLeaf = Start;
		Nodes[NodeIndex].LeafCount = LeafCount;
		Nodes[NodeIndex].FirstPrimitivePacket = static_cast<int32>(PrimitivePackets.size());
		Nodes[NodeIndex].PrimitivePacketCount = (LeafCount + WorldBVHLeafPacketSize - 1) / WorldBVHLeafPacketSize;

		for (int32 PacketIndex = 0; PacketIndex < Nodes[NodeIndex].PrimitivePacketCount; ++PacketIndex)
		{
			const int32 PacketStart = Start + PacketIndex * WorldBVHLeafPacketSize;
			const int32 PacketEnd = std::min(PacketStart + WorldBVHLeafPacketSize, End);

			FPrimitivePacket Packet{};
			Packet.PrimitiveCount = PacketEnd - PacketStart;

			for (int32 LocalIndex = 0; LocalIndex < WorldBVHLeafPacketSize; ++LocalIndex)
			{
				if (LocalIndex < Packet.PrimitiveCount)
				{
					const int32 LeafIndex = PacketStart + LocalIndex;
					const FBoundingBox& LeafBounds = Leaves[LeafIndex].Bounds;
					Packet.PrimitiveIndices[LocalIndex] = LeafIndex;
					Packet.MinX[LocalIndex] = LeafBounds.Min.X;
					Packet.MinY[LocalIndex] = LeafBounds.Min.Y;
					Packet.MinZ[LocalIndex] = LeafBounds.Min.Z;
					Packet.MaxX[LocalIndex] = LeafBounds.Max.X;
					Packet.MaxY[LocalIndex] = LeafBounds.Max.Y;
					Packet.MaxZ[LocalIndex] = LeafBounds.Max.Z;
				}
				else
				{
					Packet.MinX[LocalIndex] = 1e30f;
					Packet.MinY[LocalIndex] = 1e30f;
					Packet.MinZ[LocalIndex] = 1e30f;
					Packet.MaxX[LocalIndex] = -1e30f;
					Packet.MaxY[LocalIndex] = -1e30f;
					Packet.MaxZ[LocalIndex] = -1e30f;
				}
			}

			PrimitivePackets.push_back(Packet);
		}

		return NodeIndex;
	}

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

	const int32 DesiredChildren = std::min<int32>(WorldBVHChildFanout, LeafCount);
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

		const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
		Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
		Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
		Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
		Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
		Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
		Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

		Nodes[NodeIndex].ChildCount++;
	}

	for (int32 i = Nodes[NodeIndex].ChildCount; i < WorldBVHChildFanout; ++i)
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
