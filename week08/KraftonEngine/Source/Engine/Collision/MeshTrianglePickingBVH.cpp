#include "Collision/MeshTrianglePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Mesh/StaticMeshAsset.h"
#include "Core/EngineTypes.h"

#include <algorithm>
#include <cfloat>

namespace
{
	FBoundingBox MakeTriangleBounds(const FVector& V0, const FVector& V1, const FVector& V2)
	{
		FBoundingBox Bounds;
		Bounds.Expand(V0);
		Bounds.Expand(V1);
		Bounds.Expand(V2);
		return Bounds;
	}
}

void FMeshTrianglePickingBVH::BuildNow(const FStaticMesh& Mesh)
{
	TriangleLeaves.clear();
	LeafPackets.clear();
	Nodes.clear();

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		return;
	}

	TriangleLeaves.reserve(Mesh.Indices.size() / 3);
	LeafPackets.reserve((Mesh.Indices.size() / 3 + 7) / 8);

	for (size_t Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
	{
		const FVector& V0 = Mesh.Vertices[Mesh.Indices[Index]].pos;
		const FVector& V1 = Mesh.Vertices[Mesh.Indices[Index + 1]].pos;
		const FVector& V2 = Mesh.Vertices[Mesh.Indices[Index + 2]].pos;

		FTriangleLeaf Leaf;
		Leaf.TriangleStartIndex = static_cast<int32>(Index);
		Leaf.Bounds = MakeTriangleBounds(V0, V1, V2);
		TriangleLeaves.push_back(Leaf);
	}

	if (!TriangleLeaves.empty())
	{
		BuildRecursive(Mesh, 0, static_cast<int32>(TriangleLeaves.size()));
	}
}

void FMeshTrianglePickingBVH::EnsureBuilt(const FStaticMesh& Mesh)
{
	if (!Nodes.empty())
	{
		return;
	}
	BuildNow(Mesh);
}

bool FMeshTrianglePickingBVH::RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const
{
	struct FTraversalEntry
	{
		int32 NodeIndex;
		float TMin;
	};

	OutHitResult = {};
	LastTraversalMetrics = {};
	if (Nodes.empty() || Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		return false;
	}

	TArray<FTraversalEntry> NodeStack;
	NodeStack.reserve(64);

	FRay LocalRay;
	LocalRay.Origin = LocalOrigin;
	LocalRay.Direction = LocalDirection;

	float RootTMin = 0.0f;
	float RootTMax = 0.0f;
	if (!FRayUtils::IntersectRayAABB(LocalRay, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootTMin, RootTMax))
	{
		return false;
	}

	const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(LocalOrigin, LocalDirection);

	NodeStack.push_back({ 0, RootTMin });

	float ClosestT = FLT_MAX;
	bool bHit = false;

	while (!NodeStack.empty())
	{
		const FTraversalEntry Entry = NodeStack.back();
		NodeStack.pop_back();
		if (Entry.TMin > ClosestT)
		{
			continue;
		}

		const FNode& Node = Nodes[Entry.NodeIndex];

		if (Node.IsLeaf())
		{
			LastTraversalMetrics.LeafPacketsTested++;
			const FTrianglePacket& Packet = LeafPackets[Node.PacketIndex];
			LastTraversalMetrics.TriangleLanesTested += Packet.TriangleCount;

			float TValues[8];
			const int32 Mask = FRayUtilsSIMD::IntersectTriangles8Precomputed(
				RayContext,
				Packet.V0X, Packet.V0Y, Packet.V0Z,
				Packet.Edge1X, Packet.Edge1Y, Packet.Edge1Z,
				Packet.Edge2X, Packet.Edge2Y, Packet.Edge2Z,
				ClosestT,
				TValues) & static_cast<int32>(Packet.LaneMask);

			if (Mask != 0)
			{
				for (int32 i = 0; i < 8; ++i)
				{
					if (((Mask >> i) & 1) && TValues[i] < ClosestT)
					{
						ClosestT = TValues[i];
						OutHitResult.bHit = true;
						OutHitResult.Distance = TValues[i];
						OutHitResult.FaceIndex = Packet.TriangleStartIndices[i];
						bHit = true;
					}
				}
			}
			continue;
		}

		LastTraversalMetrics.InternalNodesVisited++;

		float TMinValues[8];
		const int32 NodeMask = FRayUtilsSIMD::IntersectAABB8(
			RayContext,
			Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
			Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
			ClosestT,
			TMinValues);
		if (NodeMask == 0)
		{
			continue;
		}

		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;

		for (int32 i = 0; i < 8; ++i)
		{
			if ((NodeMask >> i) & 1)
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

	return bHit;
}

int32 FMeshTrianglePickingBVH::BuildRecursive(const FStaticMesh& Mesh, int32 Start, int32 End)
{
	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();

	FBoundingBox Bounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		Bounds.Expand(TriangleLeaves[LeafIndex].Bounds.Min);
		Bounds.Expand(TriangleLeaves[LeafIndex].Bounds.Max);
	}
	Nodes[NodeIndex].Bounds = Bounds;

	constexpr int32 MaxLeafSize = 8;
	const int32 LeafCount = End - Start;
	if (LeafCount <= MaxLeafSize)
	{
		Nodes[NodeIndex].FirstLeaf = Start;
		Nodes[NodeIndex].LeafCount = LeafCount;
		Nodes[NodeIndex].PacketIndex = static_cast<int32>(LeafPackets.size());

		FTrianglePacket Packet{};
		Packet.TriangleCount = static_cast<uint32>(LeafCount);

		for (int32 i = 0; i < 8; ++i)
		{
			if (i < LeafCount)
			{
				const int32 TriStartIndex = TriangleLeaves[Start + i].TriangleStartIndex;
				const FVector& V0 = Mesh.Vertices[Mesh.Indices[TriStartIndex]].pos;
				const FVector& V1 = Mesh.Vertices[Mesh.Indices[TriStartIndex + 1]].pos;
				const FVector& V2 = Mesh.Vertices[Mesh.Indices[TriStartIndex + 2]].pos;
				const FVector Edge1 = V1 - V0;
				const FVector Edge2 = V2 - V0;

				Packet.V0X[i] = V0.X; Packet.V0Y[i] = V0.Y; Packet.V0Z[i] = V0.Z;
				Packet.Edge1X[i] = Edge1.X; Packet.Edge1Y[i] = Edge1.Y; Packet.Edge1Z[i] = Edge1.Z;
				Packet.Edge2X[i] = Edge2.X; Packet.Edge2Y[i] = Edge2.Y; Packet.Edge2Z[i] = Edge2.Z;
				Packet.TriangleStartIndices[i] = TriStartIndex;
				Packet.LaneMask |= (1u << i);
			}
			else
			{
				Packet.V0X[i] = Packet.V0Y[i] = Packet.V0Z[i] = 0.0f;
				Packet.Edge1X[i] = Packet.Edge1Y[i] = Packet.Edge1Z[i] = 0.0f;
				Packet.Edge2X[i] = Packet.Edge2Y[i] = Packet.Edge2Z[i] = 0.0f;
			}
		}

		LeafPackets.push_back(Packet);
		return NodeIndex;
	}

	FBoundingBox CentroidBounds;
	for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
	{
		CentroidBounds.Expand(TriangleLeaves[LeafIndex].Bounds.GetCenter());
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
		TriangleLeaves.begin() + Start,
		TriangleLeaves.begin() + End,
		[SplitAxis](const FTriangleLeaf& A, const FTriangleLeaf& B)
		{
			const FVector CenterA = A.Bounds.GetCenter();
			const FVector CenterB = B.Bounds.GetCenter();
			if (SplitAxis == 1) return CenterA.Y < CenterB.Y;
			if (SplitAxis == 2) return CenterA.Z < CenterB.Z;
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

		const int32 ChildIdx = BuildRecursive(Mesh, RangeStart, RangeEnd);
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
