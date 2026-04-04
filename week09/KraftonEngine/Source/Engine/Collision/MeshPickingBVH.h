#pragma once

#include "Core/CoreTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;

class FMeshPickingBVH
{
public:
	// 메시 정점/인덱스가 바뀌었을 때 캐시된 triangle BVH를 무효화합니다.
	void MarkDirty();
	// 메시의 모든 삼각형을 leaf로 수집한 뒤 로컬 공간 BVH를 즉시 다시 빌드합니다.
	void BuildNow(const FStaticMesh& Mesh);
	// BVH가 무효화된 경우에만 재빌드를 수행합니다.
	void EnsureBuilt(const FStaticMesh& Mesh);
	// 로컬 공간 ray로 BVH를 순회해 가장 가까운 삼각형 hit를 찾습니다.
	bool RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const;

private:
	struct FTriangleLeaf
	{
		FBoundingBox Bounds;
		// Mesh.Indices에서 이 삼각형이 시작하는 위치입니다.
		int32 TriangleStartIndex = 0;
	};

	struct FNode
	{
		FBoundingBox Bounds;
		int32 LeftChild = -1;
		int32 RightChild = -1;
		int32 FirstLeaf = 0;
		int32 LeafCount = 0;

		bool IsLeaf() const { return LeftChild < 0 && RightChild < 0; }
	};

	int32 BuildRecursive(int32 Start, int32 End);

	bool bDirty = true;
	// 삼각형 단위 leaf 배열입니다. BVH 리프는 이 배열의 연속 구간을 가리킵니다.
	TArray<FTriangleLeaf> TriangleLeaves;
	// 루트부터 자식까지 연속 저장한 BVH 노드 배열입니다.
	TArray<FNode> Nodes;
};
