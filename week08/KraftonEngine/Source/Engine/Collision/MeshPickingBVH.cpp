#include "Collision/MeshPickingBVH.h"

#include "Collision/RayUtils.h"
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

void FMeshPickingBVH::MarkDirty()
{
	bDirty = true;
}

void FMeshPickingBVH::BuildNow(const FStaticMesh& Mesh)
{
	TriangleLeaves.clear();
	Nodes.clear();

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		bDirty = false;
		return;
	}

	TriangleLeaves.reserve(Mesh.Indices.size() / 3);

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
		BuildRecursive(0, static_cast<int32>(TriangleLeaves.size()));
	}

	bDirty = false;
}

void FMeshPickingBVH::EnsureBuilt(const FStaticMesh& Mesh)
{
	if (!bDirty)
	{
		return;
	}

	BuildNow(Mesh);
}

bool FMeshPickingBVH::RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const
{
	OutHitResult = {};
	if (Nodes.empty() || Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		return false;
	}

	FRay LocalRay;
	LocalRay.Origin = LocalOrigin;
	LocalRay.Direction = LocalDirection;

	TArray<int32> NodeStack;
	NodeStack.push_back(0);

	float ClosestT = FLT_MAX;
	bool bHit = false;

	while (!NodeStack.empty())
	{
		const int32 NodeIndex = NodeStack.back();
		NodeStack.pop_back();

		const FNode& Node = Nodes[NodeIndex];
		if (!FRayUtils::CheckRayAABB(LocalRay, Node.Bounds.Min, Node.Bounds.Max))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			for (int32 LeafIndex = Node.FirstLeaf; LeafIndex < Node.FirstLeaf + Node.LeafCount; ++LeafIndex)
			{
				const FTriangleLeaf& Leaf = TriangleLeaves[LeafIndex];
				const int32 TriangleStart = Leaf.TriangleStartIndex;

				const FVector& V0 = Mesh.Vertices[Mesh.Indices[TriangleStart]].pos;
				const FVector& V1 = Mesh.Vertices[Mesh.Indices[TriangleStart + 1]].pos;
				const FVector& V2 = Mesh.Vertices[Mesh.Indices[TriangleStart + 2]].pos;

				float T = 0.0f;
				if (FRayUtils::IntersectTriangle(LocalOrigin, LocalDirection, V0, V1, V2, T) && T < ClosestT)
				{
					ClosestT = T;
					OutHitResult.bHit = true;
					OutHitResult.Distance = T;
					OutHitResult.FaceIndex = TriangleStart;
					bHit = true;
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

	return bHit;
}

int32 FMeshPickingBVH::BuildRecursive(int32 Start, int32 End)
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

	const int32 Mid = Start + LeafCount / 2;
	std::nth_element(
		TriangleLeaves.begin() + Start,
		TriangleLeaves.begin() + Mid,
		TriangleLeaves.begin() + End,
		[SplitAxis](const FTriangleLeaf& A, const FTriangleLeaf& B)
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
