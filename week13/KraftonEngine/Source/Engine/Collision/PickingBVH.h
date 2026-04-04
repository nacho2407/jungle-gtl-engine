#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"

class AActor;
class UPrimitiveComponent;

class FPickingBVH
{
public:
	//월드 상태나 픽킹 대상 변화로 인해 캐시된 트리를 무효화합니다. -> TODO: 최적화 여부 비교해보기
	void MarkDirty();
	//현재 월드의 actor 목록을 기준으로 픽킹 트리를 즉시 다시 만듭니다.
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
		AActor* Owner = nullptr;
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
	TArray<FLeaf> Leaves;
	TArray<FNode> Nodes;
};
