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
	void MarkDirty();
	void BuildNow(const TArray<AActor*>& Actors);
	void EnsureBuilt(const TArray<AActor*>& Actors);
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
