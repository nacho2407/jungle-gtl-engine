#include "Collision/MeshTrianglePickingBVH.h"

#include "Collision/RayUtils.h"
#include "Mesh/StaticMeshAsset.h"
#include "Core/EngineTypes.h"

#include <algorithm>
#include <cfloat>
#include <immintrin.h>

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
 * 내부 노드 AABB 검사와 리프 노드 삼각형 교차 검사 모두 AVX2 SIMD를 사용합니다.
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

	auto safe_inv = [](float d) {
		return std::abs(d) > 1e-8f ? 1.0f / d : 1e30f;
	};

	__m256 ray_org_x = _mm256_set1_ps(LocalOrigin.X);
	__m256 ray_org_y = _mm256_set1_ps(LocalOrigin.Y);
	__m256 ray_org_z = _mm256_set1_ps(LocalOrigin.Z);
	__m256 ray_dir_x = _mm256_set1_ps(LocalDirection.X);
	__m256 ray_dir_y = _mm256_set1_ps(LocalDirection.Y);
	__m256 ray_dir_z = _mm256_set1_ps(LocalDirection.Z);
	__m256 inv_dir_x = _mm256_set1_ps(safe_inv(LocalDirection.X));
	__m256 inv_dir_y = _mm256_set1_ps(safe_inv(LocalDirection.Y));
	__m256 inv_dir_z = _mm256_set1_ps(safe_inv(LocalDirection.Z));

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
			// 리프 노드의 삼각형 8개를 동시에 검사 (AVX2 Möller-Trumbore)
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

			__m256 V0x = _mm256_load_ps(v0x), V0y = _mm256_load_ps(v0y), V0z = _mm256_load_ps(v0z);
			__m256 V1x = _mm256_load_ps(v1x), V1y = _mm256_load_ps(v1y), V1z = _mm256_load_ps(v1z);
			__m256 V2x = _mm256_load_ps(v2x), V2y = _mm256_load_ps(v2y), V2z = _mm256_load_ps(v2z);

			__m256 e1x = _mm256_sub_ps(V1x, V0x), e1y = _mm256_sub_ps(V1y, V0y), e1z = _mm256_sub_ps(V1z, V0z);
			__m256 e2x = _mm256_sub_ps(V2x, V0x), e2y = _mm256_sub_ps(V2y, V0y), e2z = _mm256_sub_ps(V2z, V0z);

			__m256 pvx = _mm256_sub_ps(_mm256_mul_ps(ray_dir_y, e2z), _mm256_mul_ps(ray_dir_z, e2y));
			__m256 pvy = _mm256_sub_ps(_mm256_mul_ps(ray_dir_z, e2x), _mm256_mul_ps(ray_dir_x, e2z));
			__m256 pvz = _mm256_sub_ps(_mm256_mul_ps(ray_dir_x, e2y), _mm256_mul_ps(ray_dir_y, e2x));

			__m256 det = _mm256_add_ps(_mm256_mul_ps(e1x, pvx), _mm256_add_ps(_mm256_mul_ps(e1y, pvy), _mm256_mul_ps(e1z, pvz)));
			__m256 abs_det = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), det);
			__m256 inv_det = _mm256_div_ps(_mm256_set1_ps(1.0f), det);

			__m256 tvx = _mm256_sub_ps(ray_org_x, V0x), tvy = _mm256_sub_ps(ray_org_y, V0y), tvz = _mm256_sub_ps(ray_org_z, V0z);
			__m256 u = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(tvx, pvx), _mm256_add_ps(_mm256_mul_ps(tvy, pvy), _mm256_mul_ps(tvz, pvz))), inv_det);

			__m256 qvx = _mm256_sub_ps(_mm256_mul_ps(tvy, e1z), _mm256_mul_ps(tvz, e1y));
			__m256 qvy = _mm256_sub_ps(_mm256_mul_ps(tvz, e1x), _mm256_mul_ps(tvx, e1z));
			__m256 qvz = _mm256_sub_ps(_mm256_mul_ps(tvx, e1y), _mm256_mul_ps(tvy, e1x));

			__m256 v = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(ray_dir_x, qvx), _mm256_add_ps(_mm256_mul_ps(ray_dir_y, qvy), _mm256_mul_ps(ray_dir_z, qvz))), inv_det);
			__m256 t = _mm256_mul_ps(_mm256_add_ps(_mm256_mul_ps(e2x, qvx), _mm256_add_ps(_mm256_mul_ps(e2y, qvy), _mm256_mul_ps(e2z, qvz))), inv_det);

			__m256 zero = _mm256_setzero_ps();
			__m256 one = _mm256_set1_ps(1.0f);
			__m256 hit_mask = _mm256_and_ps(
				_mm256_cmp_ps(abs_det, _mm256_set1_ps(1e-8f), _CMP_GT_OQ),
				_mm256_and_ps(_mm256_cmp_ps(u, zero, _CMP_GE_OQ),
				_mm256_and_ps(_mm256_cmp_ps(u, one, _CMP_LE_OQ),
				_mm256_and_ps(_mm256_cmp_ps(v, zero, _CMP_GE_OQ),
				_mm256_and_ps(_mm256_cmp_ps(_mm256_add_ps(u, v), one, _CMP_LE_OQ),
				_mm256_and_ps(_mm256_cmp_ps(t, zero, _CMP_GT_OQ),
				_mm256_cmp_ps(t, _mm256_set1_ps(ClosestT), _CMP_LT_OQ))))))
			);

			int mask = _mm256_movemask_ps(hit_mask);
			if (mask != 0)
			{
				float TValues[8];
				_mm256_storeu_ps(TValues, t);
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

		// 내부 노드 자식 8개 AABB 동시 검사 (Broad Phase와 동일 로직)
		__m256 min_x = _mm256_loadu_ps(Node.ChildMinX);
		__m256 min_y = _mm256_loadu_ps(Node.ChildMinY);
		__m256 min_z = _mm256_loadu_ps(Node.ChildMinZ);
		__m256 max_x = _mm256_loadu_ps(Node.ChildMaxX);
		__m256 max_y = _mm256_loadu_ps(Node.ChildMaxY);
		__m256 max_z = _mm256_loadu_ps(Node.ChildMaxZ);

		__m256 t1_x = _mm256_mul_ps(_mm256_sub_ps(min_x, ray_org_x), inv_dir_x);
		__m256 t2_x = _mm256_mul_ps(_mm256_sub_ps(max_x, ray_org_x), inv_dir_x);
		__m256 t1_y = _mm256_mul_ps(_mm256_sub_ps(min_y, ray_org_y), inv_dir_y);
		__m256 t2_y = _mm256_mul_ps(_mm256_sub_ps(max_y, ray_org_y), inv_dir_y);
		__m256 t1_z = _mm256_mul_ps(_mm256_sub_ps(min_z, ray_org_z), inv_dir_z);
		__m256 t2_z = _mm256_mul_ps(_mm256_sub_ps(max_z, ray_org_z), inv_dir_z);

		__m256 t_min = _mm256_max_ps(_mm256_max_ps(_mm256_min_ps(t1_x, t2_x), _mm256_min_ps(t1_y, t2_y)), _mm256_min_ps(t1_z, t2_z));
		__m256 t_max = _mm256_min_ps(_mm256_min_ps(_mm256_max_ps(t1_x, t2_x), _mm256_max_ps(t1_y, t2_y)), _mm256_max_ps(t1_z, t2_z));

		__m256 node_hit_mask = _mm256_and_ps(
			_mm256_cmp_ps(t_min, t_max, _CMP_LE_OQ),
			_mm256_and_ps(
				_mm256_cmp_ps(t_max, _mm256_setzero_ps(), _CMP_GE_OQ),
				_mm256_cmp_ps(t_min, _mm256_set1_ps(ClosestT), _CMP_LT_OQ)
			)
		);

		int node_mask = _mm256_movemask_ps(node_hit_mask);
		if (node_mask == 0) continue;

		FTraversalEntry ChildEntries[8];
		int32 ChildEntryCount = 0;
		float TMinValues[8];
		_mm256_storeu_ps(TMinValues, t_min);

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
		Nodes[NodeIndex].ChildMaxX[i] = -1e30f;
	}

	return NodeIndex;
}