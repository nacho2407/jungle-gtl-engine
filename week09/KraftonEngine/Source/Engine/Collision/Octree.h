#pragma once
#include "Engine/Core/CoreTypes.h"
#include "Engine/Math/Vector.h"
#include "Component/PrimitiveComponent.h"
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
	inline bool IsLeaf() const { return Children == nullptr; }

	const FBoundingBox& GetBounds() const { return BoundOctree; }
	const FOctree* GetChildren() const { return Children; }

	// 근접 쿼리 - 프리미티브 반환
	UPrimitiveComponent* FindNearestPrimitive(const FVector& Pos);
	TArray<UPrimitiveComponent*> FindNearestPrimitiveList(const FVector& Pos, const FVector& QueryExtent, uint32 Count);

	void QueryAABB(const FBoundingBox& QueryBox, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const;
	void QueryRay(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const;

	void Reset(const FBoundingBox& InBounds, uint32 InDepth = 0);
private:
	FBoundingBox BoundOctree;

	FOctree* Children = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveList;

	uint32 Depth;
};

