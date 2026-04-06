#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"

class AActor;
class UPrimitiveComponent;
class UStaticMeshComponent;

class FWorldPrimitivePickingBVH
{
public:
	//월드 상태나 picking 대상 변화로 인해 캐시된 트리를 무효화합니다. -> TODO: 최적화 여부 비교해보기
	void MarkDirty();
	//현재 월드의 actor 목록을 기준으로 picking 트리를 즉시 다시 만듭니다.
	void BuildNow(const TArray<AActor*>& Actors);
	//트리가 무효화된 경우에만 재빌드를 수행합니다.
	void EnsureBuilt(const TArray<AActor*>& Actors);
	//트리를 순회해 가장 가까운 primitive hit 결과를 찾습니다.
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;

private:
	struct FLeaf
	{
		FBoundingBox Bounds;
		UPrimitiveComponent* Primitive = nullptr;
		UStaticMeshComponent* StaticMeshPrimitive = nullptr;
		AActor* Owner = nullptr;
	};

	struct FNode
	{
		FBoundingBox Bounds;
		//최대 자식 분기는 8번. 이게 효율적인가?
		int32 Children[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

		// SIMD(AVX) 최적화를 위한 자식 노드들의 AABB 데이터 (SOA 구조)
		// 부모 노드에 미리 모아두어 Raycast 시 Gather 오버헤드를 없앱니다.
		alignas(32) float ChildMinX[8];
		alignas(32) float ChildMinY[8];
		alignas(32) float ChildMinZ[8];
		alignas(32) float ChildMaxX[8];
		alignas(32) float ChildMaxY[8];
		alignas(32) float ChildMaxZ[8];

		int32 ChildCount = 0;
		int32 FirstLeaf = 0;
		int32 LeafCount = 0;
		int32 FirstPrimitivePacket = 0;
		int32 PrimitivePacketCount = 0;

		bool IsLeaf() const { return ChildCount == 0; }
	};

	struct alignas(32) FPrimitivePacket
	{
		int32 PrimitiveIndices[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		alignas(32) float MinX[8];
		alignas(32) float MinY[8];
		alignas(32) float MinZ[8];
		alignas(32) float MaxX[8];
		alignas(32) float MaxY[8];
		alignas(32) float MaxZ[8];
		int32 PrimitiveCount = 0;
	};

	int32 BuildRecursive(int32 Start, int32 End);

	bool bDirty = true;
	TArray<FLeaf> Leaves;
	TArray<FNode> Nodes;
	TArray<FPrimitivePacket> PrimitivePackets;
};
