#include "Collision/WorldPrimitivePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/Profiling/PlatformTime.h"
#include "GameFramework/AActor.h"

#include <algorithm>

namespace
{
	constexpr int32 WorldBVHChildFanout = 8;
	constexpr int32 WorldBVHLeafPacketSize = 8;
	constexpr int32 WorldBVHMaxLeafSize = 16;
	constexpr int32 WorldBVHMaxTraversalStack = 128;
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
	if (!FRayUtils::IntersectRayAABB(Ray, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootTMin, RootTMax))
	{
		return false;
	}

	const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(Ray.Origin, Ray.Direction);

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

			for (int32 I = 1; I < PrimitiveEntryCount; ++I)
			{
				FTraversalEntry Key = PrimitiveEntries[I];
				int32 J = I - 1;
				while (J >= 0 && PrimitiveEntries[J].TMin < Key.TMin)
				{
					PrimitiveEntries[J + 1] = PrimitiveEntries[J];
					--J;
				}
				PrimitiveEntries[J + 1] = Key;
			}

			for (int32 EntryIndex = 0; EntryIndex < PrimitiveEntryCount; ++EntryIndex)
			{
				const FLeaf& Leaf = Leaves[PrimitiveEntries[EntryIndex].NodeIndex];
				FHitResult CandidateHit{};
				const uint64 NarrowPhaseStart = FPlatformTime::Cycles64();
				LastTraversalMetrics.NarrowPhaseCalls++;

				if (Leaf.Primitive->LineTraceComponent(Ray, CandidateHit) &&
					CandidateHit.Distance < OutHitResult.Distance)
				{
					OutHitResult = CandidateHit;
					OutActor = Leaf.Owner;
				}

				LastTraversalMetrics.NarrowPhaseMs +=
					FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64() - NarrowPhaseStart);

				const FPrimitivePickingMetrics& PrimitiveMetrics = Leaf.Primitive->GetLastPickingMetrics();
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
