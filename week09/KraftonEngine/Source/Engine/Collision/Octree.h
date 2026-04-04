#pragma once
#include "Engine/Core/CoreTypes.h"
#include "Engine/Math/Vector.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Culling/ConvexVolume.h"
#include <memory>

constexpr int32 MAX_DEPTH = 5;
constexpr int32 MAX_SIZE = 32;

class FFrustum;

class FOctree
{
public:
	FOctree();
	FOctree(const FBoundingBox& BoundOctree, const uint32& depth);

	~FOctree();

	bool Insert(UPrimitiveComponent * InPrimitivie);
	void SubDivide();

	bool Remove(const UPrimitiveComponent * InPrimitivie);  
	void TryMerge();

	bool HasPrimitive(const UPrimitiveComponent* p);
	void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPremitive);
	inline bool IsLeaf() const { return Children.empty(); }

	const FBoundingBox& GetBounds() const { return BoundOctree; }
	const TArray<FOctree*>& GetChildren() const { return Children; }

	TArray<UPrimitiveComponent*> FindNearestPrimitiveList(const FVector& Pos, const FVector& QueryExtent, uint32 Count);

	void QueryAABB(const FBoundingBox& QueryBox, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryFrustum(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryRay(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const;

	void Reset(const FBoundingBox& InBounds, uint32 InDepth = 0);
private:
	FBoundingBox BoundOctree;

	TArray<FOctree*> Children;
	TArray<UPrimitiveComponent*> PrimitiveList;

	uint32 Depth;
};

