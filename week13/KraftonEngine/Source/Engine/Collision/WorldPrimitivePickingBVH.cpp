#include "Collision/WorldPrimitivePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Component/PrimitiveComponent.h"
#include "Engine/Profiling/PlatformTime.h"
#include "GameFramework/AActor.h"

#include <algorithm>

void FWorldPrimitivePickingBVH::MarkDirty()
{
	bDirty = true;
}

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
		int32 NodeIndex;
		float TMin;
	};

	OutHitResult = {};
	OutActor = nullptr;
	LastTraversalMetrics = {};
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

		const FNode& Node = Nodes[Entry.NodeIndex];

		if (Node.IsLeaf())
		{
			LastTraversalMetrics.LeafNodesVisited++;

			float PrimitiveTMinValues[8];
			const int32 PrimitiveMask = FRayUtilsSIMD::IntersectAABB8(
				RayContext,
				Node.LeafMinX, Node.LeafMinY, Node.LeafMinZ,
				Node.LeafMaxX, Node.LeafMaxY, Node.LeafMaxZ,
				OutHitResult.Distance,
				PrimitiveTMinValues);
			LastTraversalMetrics.PrimitiveAABBTests += static_cast<uint32>(Node.LeafCount);

			if (PrimitiveMask == 0)
			{
				continue;
			}

			FTraversalEntry PrimitiveEntries[8];
			int32 PrimitiveEntryCount = 0;
			for (int32 i = 0; i < Node.LeafCount; ++i)
			{
				if ((PrimitiveMask >> i) & 1)
				{
					LastTraversalMetrics.PrimitiveAABBHits++;
					PrimitiveEntries[PrimitiveEntryCount++] = { Node.LeafPrimitiveIndices[i], PrimitiveTMinValues[i] };
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

		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;

		for (int32 i = 0; i < 8; ++i)
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

		for (int32 I = 0; I < ChildEntryCount; ++I)
		{
			NodeStack.push_back(ChildEntries[I]);
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

	constexpr int32 MaxLeafSize = 8;
	const int32 LeafCount = End - Start;
	if (LeafCount <= MaxLeafSize)
	{
		Nodes[NodeIndex].FirstLeaf = Start;
		Nodes[NodeIndex].LeafCount = LeafCount;

		for (int32 LocalLeafIndex = 0; LocalLeafIndex < 8; ++LocalLeafIndex)
		{
			if (LocalLeafIndex < LeafCount)
			{
				const int32 LeafIndex = Start + LocalLeafIndex;
				const FBoundingBox& LeafBounds = Leaves[LeafIndex].Bounds;
				Nodes[NodeIndex].LeafPrimitiveIndices[LocalLeafIndex] = LeafIndex;
				Nodes[NodeIndex].LeafMinX[LocalLeafIndex] = LeafBounds.Min.X;
				Nodes[NodeIndex].LeafMinY[LocalLeafIndex] = LeafBounds.Min.Y;
				Nodes[NodeIndex].LeafMinZ[LocalLeafIndex] = LeafBounds.Min.Z;
				Nodes[NodeIndex].LeafMaxX[LocalLeafIndex] = LeafBounds.Max.X;
				Nodes[NodeIndex].LeafMaxY[LocalLeafIndex] = LeafBounds.Max.Y;
				Nodes[NodeIndex].LeafMaxZ[LocalLeafIndex] = LeafBounds.Max.Z;
			}
			else
			{
				Nodes[NodeIndex].LeafMinX[LocalLeafIndex] = 1e30f;
				Nodes[NodeIndex].LeafMinY[LocalLeafIndex] = 1e30f;
				Nodes[NodeIndex].LeafMinZ[LocalLeafIndex] = 1e30f;
				Nodes[NodeIndex].LeafMaxX[LocalLeafIndex] = -1e30f;
				Nodes[NodeIndex].LeafMaxY[LocalLeafIndex] = -1e30f;
				Nodes[NodeIndex].LeafMaxZ[LocalLeafIndex] = -1e30f;
			}
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

		const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
		Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
		Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
		Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
		Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
		Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
		Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

		Nodes[NodeIndex].ChildCount++;
	}

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
