#pragma once

#include "Core/CoreTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;

class FMeshPickingBVH
{
public:
	void MarkDirty();
	void BuildNow(const FStaticMesh& Mesh);
	void EnsureBuilt(const FStaticMesh& Mesh);
	bool RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const;

private:
	struct FTriangleLeaf
	{
		FBoundingBox Bounds;
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
	TArray<FTriangleLeaf> TriangleLeaves;
	TArray<FNode> Nodes;
};
