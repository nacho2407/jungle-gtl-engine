#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Engine/Core/EngineTypes.h"
#include "Engine/Render/Culling/ConvexVolume.h"

class UPrimitiveComponent;
class AActor;
class FOcclusionCulling;
struct FMatrix;
struct FVector;

class FOcclusionBVH
{
public:
	// ── 설정 ──
	static constexpr int32 ClusterSize = 32;   // 클러스터당 기본 primitive 수
	static constexpr int32 ClusterMaxSize = 64;  // 클러스터 최대 허용 수 (분할 임계)
	static constexpr int32 MaxDynamicCount = 512;  // 동적 리스트 임계 → 초과 시 전체 리빌드
	static constexpr int32 MaxBranchFactor = 8;    // 노드당 최대 자식 수

	// ── 빌드 ──

	// 전체 primitive 목록으로 정적 BVH를 처음부터 빌드
	void Build(const TArray<AActor*>& Actors);

	// 정적 BVH가 유효한 상태에서 동적 리스트만 반영하여 경량 갱신
	// 동적 리스트가 MaxDynamicCount를 초과하면 내부적으로 FullRebuild 호출
	void Rebuild();

	// ── Dirty 관리 ──

	// 액터가 이동/변형되었을 때 호출 → 동적 리스트로 이동 예약
	void MarkDirty(AActor* Actor);

	// 새 액터 추가 → 동적 리스트에 삽입
	void AddActor(AActor* Actor);

	// 액터 제거 → 정적 BVH + 동적 리스트 양쪽에서 제거
	void RemoveActor(AActor* Actor);

	// ── 쿼리 ──

	// Frustum + Occlusion 통합 쿼리 (메인 진입점)
	// 정적 BVH 순회 + 동적 리스트 개별 테스트
	void QueryFrustumOcclusion(
		const FConvexVolume& ConvexVolume,
		FOcclusionCulling& Occlusion,
		const FMatrix& ViewProj,
		const FVector& CameraPos,
		TArray<UPrimitiveComponent*>& OutPrimitives) const;

	// ── 접근자 ──

	bool   IsBuilt() const { return !Nodes.empty(); }
	int32  GetNodeCount() const { return static_cast<int32>(Nodes.size()); }
	int32  GetClusterCount() const { return static_cast<int32>(Clusters.size()); }
	int32  GetDynamicCount() const { return static_cast<int32>(DynamicPrimitives.size()); }
	FBoundingBox GetClusterBounds(int32 Index) const { return Clusters[Index].Bounds; }

private:
	// ── 내부 구조 ──

	struct FCluster
	{
		FBoundingBox Bounds;
		int32 FirstPrimitive = 0;   // Primitives 배열 내 시작 인덱스
		int32 PrimitiveCount = 0;
	};

	struct FNode
	{
		FBoundingBox Bounds;
		int32 Children[MaxBranchFactor] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		int32 ChildCount = 0;

		// Leaf일 때만 유효
		int32 FirstCluster = 0;
		int32 ClusterCount = 0;

		bool IsLeaf() const { return ChildCount == 0; }
	};

	// ── 빌드 헬퍼 ──

	// Primitive 수집 → 클러스터링 → BVH 빌드
	void CollectPrimitives(const TArray<AActor*>& Actors);
	void BuildClusters();
	void BuildClustersRecursive(int32 Start, int32 End);
	int32 BuildRecursive(int32 ClusterStart, int32 ClusterEnd);

	// 동적 리스트가 임계치 초과 시 정적 BVH + 동적을 합쳐 전체 리빌드
	void FullRebuild();

	// 클러스터의 AABB를 primitive 목록에서 재계산
	FBoundingBox ComputeClusterBounds(const FCluster& Cluster) const;

	// ── 순회 헬퍼 ──

	// BVH 노드 재귀 순회 (front-to-back + frustum + occlusion)
	void TraverseNode(
		int32 NodeIndex,
		const FConvexVolume& ConvexVolume,
		FOcclusionCulling& Occlusion,
		const FMatrix& ViewProj,
		const FVector& CameraPos,
		TArray<UPrimitiveComponent*>& OutPrimitives) const;

	// 동적 리스트 개별 frustum + occlusion 테스트
	void TestDynamicPrimitives(
		const FConvexVolume& ConvexVolume,
		FOcclusionCulling& Occlusion,
		const FMatrix& ViewProj,
		TArray<UPrimitiveComponent*>& OutPrimitives) const;

	// ── 데이터 ──

	// 정적 BVH
	TArray<UPrimitiveComponent*> Primitives;   // 클러스터가 참조하는 primitive 풀
	TArray<FCluster>             Clusters;
	TArray<FNode>                Nodes;

	// 동적 (이동/추가된 액터의 primitive)
	TArray<UPrimitiveComponent*> DynamicPrimitives;

	// Dirty 추적
	TArray<AActor*>              DirtyActors;

	// 전체 리빌드용 원본 액터 목록 캐시
	TArray<AActor*>              CachedActors;
};
