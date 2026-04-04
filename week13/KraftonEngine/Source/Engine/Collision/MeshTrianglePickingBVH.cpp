#include "Collision/MeshTrianglePickingBVH.h"

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

//void FMeshTrianglePickingBVH::MarkDirty()
//{
//	bDirty = true;
//}

/**
 * 메시의 모든 삼각형을 leaf로 수집하고, triangle 단위 BVH를 새로 빌드합니다.
 * 현재 구현에서는 static mesh asset이 고정된다고 보고 첫 빌드 시 전체 트리를 생성합니다.
 *
 * \param Mesh BVH를 구성할 원본 정적 메시 데이터
 */
void FMeshTrianglePickingBVH::BuildNow(const FStaticMesh& Mesh)
{
	TriangleLeaves.clear();
	Nodes.clear();

	// 삼각형이 하나도 없으면 트리를 만들 필요가 없습니다.
	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		return;
	}

	TriangleLeaves.reserve(Mesh.Indices.size() / 3);

	// 메시의 각 삼각형을 leaf로 변환해 triangle 단위 AABB를 미리 계산합니다.
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
 *
 * \param Mesh BVH 생성에 사용할 원본 정적 메시 데이터
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
 * 내부 노드에서는 AABB 교차만 검사하고, leaf에 도달했을 때만 실제 triangle 교차를 수행합니다.
 *
 * \param LocalOrigin 메시 로컬 공간 기준 ray 시작점
 * \param LocalDirection 메시 로컬 공간 기준 ray 방향
 * \param Mesh 교차 검사에 사용할 메시 데이터
 * \param OutHitResult 가장 가까운 hit 결과
 * \return 하나 이상의 삼각형과 교차하면 true
 */
bool FMeshTrianglePickingBVH::RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const
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

	// 재귀 대신 명시적 스택으로 순회해 AABB에 걸리는 노드만 내려갑니다.
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
			// leaf 노드는 연속된 삼각형 구간을 보관하므로 작은 범위만 정밀 교차 검사하면 됩니다.
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

/**
 * 구간 내 triangle leaf들로부터 BVH 노드를 재귀적으로 구성합니다.
 *
 * \param Start leaf 구간 시작 인덱스
 * \param End leaf 구간 끝 다음 인덱스
 * \return 생성된 노드의 인덱스
 */
int32 FMeshTrianglePickingBVH::BuildRecursive(int32 Start, int32 End)
{
	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();

	// 현재 구간에 포함된 모든 삼각형 leaf를 감싸는 노드 bounds를 계산합니다.
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

	// centroid가 가장 넓게 퍼진 축을 고르고, 그 축의 median 기준으로 반씩 나눕니다.
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
	// 삼각형들을 SplitAxis을 기준으로 분할해 각기 파티션을 만듭니다.
	std::nth_element(
		TriangleLeaves.begin() + Start,
		TriangleLeaves.begin() + Mid, //전체를 정렬하는 대신 Mid 위치만 일단 올바르도록 파티션 수행
		TriangleLeaves.begin() + End,

		//비교용 람다, SplitAxis에 따라 다른 축으로 비교
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
		}
	);

	Nodes[NodeIndex].LeftChild = BuildRecursive(Start, Mid);
	Nodes[NodeIndex].RightChild = BuildRecursive(Mid, End);
	return NodeIndex;
}
