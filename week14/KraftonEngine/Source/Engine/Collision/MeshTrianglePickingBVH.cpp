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

/**
 * 메시의 모든 삼각형을 leaf로 수집하고, triangle 단위 BVH를 새로 빌드합니다.
 * 8분기 트리 구조를 사용하여 계층 깊이를 최적화합니다.
 */
void FMeshTrianglePickingBVH::BuildNow(const FStaticMesh& Mesh)
{
	TriangleLeaves.clear();
	Nodes.clear();

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
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
}

/**
 * mesh BVH가 아직 만들어지지 않았을 경우 빌드합니다.
 */
void FMeshTrianglePickingBVH::EnsureBuilt(const FStaticMesh& Mesh)
{
	if (!Nodes.empty())
	{
		return;
	}
	BuildNow(Mesh);
}

/**
 * 로컬 공간 ray로 mesh BVH를 순회하면서 가장 가까운 삼각형 hit를 찾습니다.
 * 내부 노드 AABB 검사와 리프 노드 삼각형 교차 검사 모두 AVX2 SIMD(256비트)를 사용합니다.
 * 
 * \param LocalOrigin
 * \param LocalDirection
 * \param Mesh
 * \param OutHitResult
 * \return 
 */
bool FMeshTrianglePickingBVH::RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const
{
	struct FTraversalEntry
	{
		int32 NodeIndex;
		float TMin;
	};

	OutHitResult = {};
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

	//mesh NBH와 scene BVH 모두 같은 ray SIMD context를 공유합니다.
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

		const int32 NodeIndex = Entry.NodeIndex;
		const FNode& Node = Nodes[NodeIndex];

		if (Node.IsLeaf())
		{
			// leaf는 최대 8개 triangle을 담고 있으므로 packet 교차 검사와 잘 맞습니다.
			alignas(32) float v0x[8], v0y[8], v0z[8];
			alignas(32) float v1x[8], v1y[8], v1z[8];
			alignas(32) float v2x[8], v2y[8], v2z[8];
			int32 indices[8];

			int32 count = Node.LeafCount;
			for (int32 i = 0; i < 8; ++i)
			{
				if (i < count)
				{
					const int32 triIdx = TriangleLeaves[Node.FirstLeaf + i].TriangleStartIndex;
					indices[i] = triIdx;
					const FVector& V0 = Mesh.Vertices[Mesh.Indices[triIdx]].pos;
					const FVector& V1 = Mesh.Vertices[Mesh.Indices[triIdx + 1]].pos;
					const FVector& V2 = Mesh.Vertices[Mesh.Indices[triIdx + 2]].pos;
					v0x[i] = V0.X; v0y[i] = V0.Y; v0z[i] = V0.Z;
					v1x[i] = V1.X; v1y[i] = V1.Y; v1z[i] = V1.Z;
					v2x[i] = V2.X; v2y[i] = V2.Y; v2z[i] = V2.Z;
				}
				else
				{
					v0x[i] = v0y[i] = v0z[i] = 1e30f; // 멀리 보내기
					v1x[i] = v1y[i] = v1z[i] = 1e30f;
					v2x[i] = v2y[i] = v2z[i] = 1e30f;
					indices[i] = -1;
				}
			}

			float TValues[8];
			const int32 mask = FRayUtilsSIMD::IntersectTriangles8(
				RayContext,
				v0x, v0y, v0z,
				v1x, v1y, v1z,
				v2x, v2y, v2z,
				ClosestT,
				TValues);
			if (mask != 0)
			{
				for (int32 i = 0; i < 8; ++i)
				{
					if ((mask >> i) & 1)
					{
						if (TValues[i] < ClosestT)
						{
							ClosestT = TValues[i];
							OutHitResult.bHit = true;
							OutHitResult.Distance = TValues[i];
							OutHitResult.FaceIndex = indices[i];
							bHit = true;
						}
					}
				}
			}
			continue;
		}

		// 내부 노드는 world BVH와 동일하게 8-way AABB packet 검사 후 거리순으로 순회합니다.
		float TMinValues[8];
		const int32 node_mask = FRayUtilsSIMD::IntersectAABB8(
			RayContext,
			Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
			Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
			ClosestT,
			TMinValues);
		if (node_mask == 0) continue;

		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;

		for (int32 i = 0; i < 8; ++i)
		{
			if ((node_mask >> i) & 1)
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

/**
 * 구간 내 triangle leaf들로부터 BVH 노드를 재귀적으로 구성합니다. (8분기 버전)
 */
int32 FMeshTrianglePickingBVH::BuildRecursive(int32 Start, int32 End)
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
	if (CentroidExtent.Y > CentroidExtent.X && CentroidExtent.Y >= CentroidExtent.Z) SplitAxis = 1;
	else if (CentroidExtent.Z > CentroidExtent.X && CentroidExtent.Z > CentroidExtent.Y) SplitAxis = 2;

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
		}
	);

	const int32 DesiredChildren = std::min<int32>(8, LeafCount);
	for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
	{
		const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
		const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
		if (RangeStart >= RangeEnd) continue;

		const int32 ChildIdx = BuildRecursive(RangeStart, RangeEnd);
		const int32 LocalChildIdx = Nodes[NodeIndex].ChildCount;
		
		Nodes[NodeIndex].Children[LocalChildIdx] = ChildIdx;
		
		// raycast 단계에서 바로 packet 검사할 수 있도록 child bounds를 SOA로 저장합니다.
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
