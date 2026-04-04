#include "Engine/Render/Culling/OcclusionBVH.h"
#include "Engine/Render/Culling/OcclusionCulling.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include <algorithm>

// ============================================================
// 거리 제곱 유틸
// ============================================================
static float DistSq(const FVector& A, const FVector& B)
{
	const FVector D = A - B;
	return D.X * D.X + D.Y * D.Y + D.Z * D.Z;
}

// ============================================================
// centroid 분포가 가장 넓은 축 인덱스 (0=X, 1=Y, 2=Z)
// ============================================================
static int32 FindLongestAxis(const FVector& Extent)
{
	if (Extent.Y > Extent.X && Extent.Y >= Extent.Z) return 1;
	if (Extent.Z > Extent.X && Extent.Z >  Extent.Y) return 2;
	return 0;
}

// ============================================================
// Primitive centroid 좌표 비교 (축별)
// ============================================================
static float PrimitiveCentroidOnAxis(UPrimitiveComponent* P, int32 Axis)
{
	const FVector C = P->GetWorldBoundingBox().GetCenter();
	if (Axis == 1) return C.Y;
	if (Axis == 2) return C.Z;
	return C.X;
}

// ============================================================
// Build — 전체 BVH 빌드
// ============================================================
void FOcclusionBVH::Build(const TArray<AActor*>& Actors)
{
	CachedActors = Actors;
	DirtyActors.clear();
	DynamicPrimitives.clear();

	CollectPrimitives(Actors);
	BuildClusters();

	Nodes.clear();
	if (!Clusters.empty())
	{
		BuildRecursive(0, static_cast<int32>(Clusters.size()));
	}
}

// ============================================================
// Rebuild — 동적 리스트 정리, 필요 시 전체 리빌드
// ============================================================
void FOcclusionBVH::Rebuild()
{
	// Dirty 액터의 primitive를 정적 BVH에서 제거하고 동적 리스트로 이동
	for (AActor* Actor : DirtyActors)
	{
		if (!Actor) continue;

		for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
		{
			if (!Prim) continue;

			// 정적 Primitives 배열에서 null 처리 (배열 재정렬 없이 무효화)
			for (int32 i = 0; i < static_cast<int32>(Primitives.size()); ++i)
			{
				if (Primitives[i] == Prim)
				{
					Primitives[i] = nullptr;
					break;
				}
			}

			// 동적 리스트에 추가 (중복 방지)
			if (Prim->IsVisible())
			{
				auto It = std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), Prim);
				if (It == DynamicPrimitives.end())
				{
					DynamicPrimitives.push_back(Prim);
				}
			}
		}
	}
	DirtyActors.clear();

	// 동적 리스트가 임계치 초과하면 전체 리빌드
	if (static_cast<int32>(DynamicPrimitives.size()) > MaxDynamicCount)
	{
		FullRebuild();
	}
}

// ============================================================
// MarkDirty — 이동/변형된 액터를 dirty로 등록
// ============================================================
void FOcclusionBVH::MarkDirty(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(DirtyActors.begin(), DirtyActors.end(), Actor);
	if (It == DirtyActors.end())
	{
		DirtyActors.push_back(Actor);
	}
}

// ============================================================
// AddActor — 새 액터를 동적 리스트에 추가
// ============================================================
void FOcclusionBVH::AddActor(AActor* Actor)
{
	if (!Actor) return;

	// CachedActors에 추가 (FullRebuild 대비)
	CachedActors.push_back(Actor);

	// 동적 리스트에 primitive 추가
	for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
	{
		if (!Prim || !Prim->IsVisible()) continue;
		DynamicPrimitives.push_back(Prim);
	}
}

// ============================================================
// RemoveActor — 정적 BVH + 동적 리스트 양쪽에서 제거
// ============================================================
void FOcclusionBVH::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	// CachedActors에서 제거
	auto CIt = std::find(CachedActors.begin(), CachedActors.end(), Actor);
	if (CIt != CachedActors.end())
	{
		*CIt = CachedActors.back();
		CachedActors.pop_back();
	}

	// 정적 Primitives에서 null 처리
	for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
	{
		if (!Prim) continue;

		for (int32 i = 0; i < static_cast<int32>(Primitives.size()); ++i)
		{
			if (Primitives[i] == Prim)
			{
				Primitives[i] = nullptr;
				break;
			}
		}

		// 동적 리스트에서도 제거
		auto DIt = std::find(DynamicPrimitives.begin(), DynamicPrimitives.end(), Prim);
		if (DIt != DynamicPrimitives.end())
		{
			*DIt = DynamicPrimitives.back();
			DynamicPrimitives.pop_back();
		}
	}

	// DirtyActors에서도 제거
	auto DAIt = std::find(DirtyActors.begin(), DirtyActors.end(), Actor);
	if (DAIt != DirtyActors.end())
	{
		*DAIt = DirtyActors.back();
		DirtyActors.pop_back();
	}
}

// ============================================================
// QueryFrustumOcclusion — 메인 쿼리 진입점
// ============================================================
void FOcclusionBVH::QueryFrustumOcclusion(
	const FConvexVolume& ConvexVolume,
	FOcclusionCulling& Occlusion,
	const FMatrix& ViewProj,
	const FVector& CameraPos,
	TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	// 1. 정적 BVH 순회
	if (!Nodes.empty())
	{
		TraverseNode(0, ConvexVolume, Occlusion, ViewProj, CameraPos, OutPrimitives);
	}

	// 2. 동적 리스트 개별 테스트
	TestDynamicPrimitives(ConvexVolume, Occlusion, ViewProj, OutPrimitives);
}

// ============================================================
// CollectPrimitives — Actor에서 visible primitive 수집
// ============================================================
void FOcclusionBVH::CollectPrimitives(const TArray<AActor*>& Actors)
{
	Primitives.clear();

	for (AActor* Actor : Actors)
	{
		if (!Actor || !Actor->IsVisible()) continue;

		for (UPrimitiveComponent* Prim : Actor->GetPrimitiveComponents())
		{
			if (!Prim || !Prim->IsVisible()) continue;

			const FBoundingBox Box = Prim->GetWorldBoundingBox();
			if (!Box.IsValid()) continue;

			Primitives.push_back(Prim);
		}
	}
}

// ============================================================
// BuildClusters — 재귀적 3축 분할로 공간 클러스터링
// ============================================================
void FOcclusionBVH::BuildClusters()
{
	Clusters.clear();

	const int32 PrimCount = static_cast<int32>(Primitives.size());
	if (PrimCount == 0) return;

	// 재귀적으로 가장 넓은 축 기준 이등분 → ClusterSize 이하가 되면 클러스터 생성
	BuildClustersRecursive(0, PrimCount);
}

void FOcclusionBVH::BuildClustersRecursive(int32 Start, int32 End)
{
	const int32 Count = End - Start;

	// ClusterSize 이하면 클러스터로 확정
	if (Count <= ClusterSize)
	{
		FCluster Cluster;
		Cluster.FirstPrimitive = Start;
		Cluster.PrimitiveCount = Count;
		Cluster.Bounds = ComputeClusterBounds(Cluster);
		Clusters.push_back(Cluster);
		return;
	}

	// centroid 분포가 가장 넓은 축으로 정렬 후 이등분
	FBoundingBox CentroidBounds;
	for (int32 i = Start; i < End; ++i)
	{
		CentroidBounds.Expand(Primitives[i]->GetWorldBoundingBox().GetCenter());
	}

	const int32 Axis = FindLongestAxis(CentroidBounds.GetExtent());

	std::sort(Primitives.begin() + Start, Primitives.begin() + End,
		[Axis](UPrimitiveComponent* A, UPrimitiveComponent* B)
		{
			return PrimitiveCentroidOnAxis(A, Axis) < PrimitiveCentroidOnAxis(B, Axis);
		});

	const int32 Mid = Start + Count / 2;
	BuildClustersRecursive(Start, Mid);
	BuildClustersRecursive(Mid, End);
}

// ============================================================
// BuildRecursive — 클러스터 구간으로 8-way BVH 노드 재귀 빌드
// ============================================================
int32 FOcclusionBVH::BuildRecursive(int32 ClusterStart, int32 ClusterEnd)
{
	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();

	// 노드 AABB = 소속 클러스터 AABB의 합집합
	FBoundingBox Bounds;
	for (int32 i = ClusterStart; i < ClusterEnd; ++i)
	{
		Bounds.Expand(Clusters[i].Bounds.Min);
		Bounds.Expand(Clusters[i].Bounds.Max);
	}
	Nodes[NodeIndex].Bounds = Bounds;

	const int32 Count = ClusterEnd - ClusterStart;

	// Leaf: MaxBranchFactor 이하면 클러스터를 직접 보관
	if (Count <= MaxBranchFactor)
	{
		Nodes[NodeIndex].FirstCluster = ClusterStart;
		Nodes[NodeIndex].ClusterCount = Count;
		return NodeIndex;
	}

	// Internal: centroid 가장 넓은 축으로 정렬 → 8-way 균등 분할
	FBoundingBox CentroidBounds;
	for (int32 i = ClusterStart; i < ClusterEnd; ++i)
	{
		CentroidBounds.Expand(Clusters[i].Bounds.GetCenter());
	}

	const int32 Axis = FindLongestAxis(CentroidBounds.GetExtent());

	std::sort(Clusters.begin() + ClusterStart, Clusters.begin() + ClusterEnd,
		[this, Axis](const FCluster& A, const FCluster& B)
		{
			const FVector CA = A.Bounds.GetCenter();
			const FVector CB = B.Bounds.GetCenter();
			if (Axis == 1) return CA.Y < CB.Y;
			if (Axis == 2) return CA.Z < CB.Z;
			return CA.X < CB.X;
		});

	const int32 DesiredChildren = std::min<int32>(MaxBranchFactor, Count);
	for (int32 i = 0; i < DesiredChildren; ++i)
	{
		const int32 RangeStart = ClusterStart + (Count * i) / DesiredChildren;
		const int32 RangeEnd   = ClusterStart + (Count * (i + 1)) / DesiredChildren;
		if (RangeStart >= RangeEnd) continue;

		Nodes[NodeIndex].Children[Nodes[NodeIndex].ChildCount++] = BuildRecursive(RangeStart, RangeEnd);
	}

	return NodeIndex;
}

// ============================================================
// FullRebuild — CachedActors 전체로 BVH 재빌드
// ============================================================
void FOcclusionBVH::FullRebuild()
{
	DynamicPrimitives.clear();
	DirtyActors.clear();

	CollectPrimitives(CachedActors);
	BuildClusters();

	Nodes.clear();
	if (!Clusters.empty())
	{
		BuildRecursive(0, static_cast<int32>(Clusters.size()));
	}
}

// ============================================================
// ComputeClusterBounds — 클러스터 내 primitive AABB 합집합
// ============================================================
FBoundingBox FOcclusionBVH::ComputeClusterBounds(const FCluster& Cluster) const
{
	FBoundingBox Bounds;
	const int32 End = Cluster.FirstPrimitive + Cluster.PrimitiveCount;

	for (int32 i = Cluster.FirstPrimitive; i < End; ++i)
	{
		if (!Primitives[i]) continue;

		const FBoundingBox Box = Primitives[i]->GetWorldBoundingBox();
		Bounds.Expand(Box.Min);
		Bounds.Expand(Box.Max);
	}

	return Bounds;
}

// ============================================================
// TraverseNode — BVH 재귀 순회 (frustum + occlusion, front-to-back)
// ============================================================
void FOcclusionBVH::TraverseNode(
	int32 NodeIndex,
	const FConvexVolume& ConvexVolume,
	FOcclusionCulling& Occlusion,
	const FMatrix& ViewProj,
	const FVector& CameraPos,
	TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	const FNode& Node = Nodes[NodeIndex];

	// 1. Frustum cull — 노드 전체가 frustum 밖이면 skip
	if (!ConvexVolume.IntersectAABB(Node.Bounds))
		return;

	// 2. Occlusion cull — 노드 AABB가 완전히 가려지면 서브트리 skip
	if (Occlusion.IsOccluded(Node.Bounds, ViewProj))
		return;

	// 3. Leaf 노드 — 클러스터 내 primitive 처리
	if (Node.IsLeaf())
	{
		for (int32 ci = Node.FirstCluster; ci < Node.FirstCluster + Node.ClusterCount; ++ci)
		{
			const FCluster& Cluster = Clusters[ci];

			// 클러스터 AABB로 한번 더 frustum + occlusion 체크
			if (!ConvexVolume.IntersectAABB(Cluster.Bounds))
				continue;
			if (Occlusion.IsOccluded(Cluster.Bounds, ViewProj))
				continue;

			// 클러스터를 occluder로 래스터라이즈
			const FVector Extent = Cluster.Bounds.GetExtent();
			if (Extent.X * Extent.Y * Extent.Z > 0.0002f)
				Occlusion.RasterizeOccluder(Cluster.Bounds, ViewProj);

			// 클러스터 내 개별 primitive 출력
			const int32 End = Cluster.FirstPrimitive + Cluster.PrimitiveCount;
			for (int32 pi = Cluster.FirstPrimitive; pi < End; ++pi)
			{
				UPrimitiveComponent* Prim = Primitives[pi];
				if (!Prim || !Prim->IsVisible()) continue;

				OutPrimitives.push_back(Prim);
			}
		}
		return;
	}

	// 4. Internal 노드 — 자식을 front-to-back 정렬 후 재귀
	int32 Order[MaxBranchFactor];
	float Dists[MaxBranchFactor];

	for (int32 i = 0; i < Node.ChildCount; ++i)
	{
		Order[i] = i;
		const FVector Center = Nodes[Node.Children[i]].Bounds.GetCenter();
		Dists[i] = DistSq(CameraPos, Center);
	}

	// 삽입 정렬 (최대 8개, 분기 예측 친화적)
	for (int32 i = 1; i < Node.ChildCount; ++i)
	{
		const int32 Key = Order[i];
		const float KeyDist = Dists[i];
		int32 j = i - 1;
		while (j >= 0 && Dists[j] > KeyDist)
		{
			Order[j + 1] = Order[j];
			Dists[j + 1] = Dists[j];
			--j;
		}
		Order[j + 1] = Key;
		Dists[j + 1] = KeyDist;
	}

	for (int32 i = 0; i < Node.ChildCount; ++i)
	{
		TraverseNode(Node.Children[Order[i]], ConvexVolume, Occlusion, ViewProj, CameraPos, OutPrimitives);
	}
}

// ============================================================
// TestDynamicPrimitives — 동적 리스트 개별 frustum + occlusion 테스트
// ============================================================
void FOcclusionBVH::TestDynamicPrimitives(
	const FConvexVolume& ConvexVolume,
	FOcclusionCulling& Occlusion,
	const FMatrix& ViewProj,
	TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	for (UPrimitiveComponent* Prim : DynamicPrimitives)
	{
		if (!Prim || !Prim->IsVisible()) continue;

		const FBoundingBox Box = Prim->GetWorldBoundingBox();
		if (!ConvexVolume.IntersectAABB(Box)) continue;
		if (Occlusion.IsOccluded(Box, ViewProj)) continue;

		// Occluder로 래스터라이즈
		const FVector Extent = Box.GetExtent();
		if (Extent.X * Extent.Y * Extent.Z > 0.0002f)
			Occlusion.RasterizeOccluder(Box, ViewProj);

		OutPrimitives.push_back(Prim);
	}
}
