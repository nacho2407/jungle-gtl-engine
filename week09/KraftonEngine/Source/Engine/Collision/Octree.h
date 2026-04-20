#pragma once
#include "Engine/Core/CoreTypes.h"
#include "Engine/Math/Vector.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"
#include <memory>

class FPrimitiveSceneProxy;

constexpr int32 MAX_DEPTH = 6;
constexpr int32 MAX_SIZE = 20;
constexpr float LooseFactor = 1.3f;

class FFrustum;

class FOctree
{
public:
    FOctree();
    FOctree(const FBoundingBox& BoundOctree, const uint32& depth, FOctree* InParent = nullptr);


    ~FOctree();

    bool Insert(UPrimitiveComponent* InPrimitivie);

    bool Remove(UPrimitiveComponent* InPrimitivie);
    bool RemoveDirect(UPrimitiveComponent* Primitive, bool bTryMergeNow = true);
    void TryMergeUpward();
    void TryMergeRecursive();

    bool HasPrimitive(const UPrimitiveComponent* p);
    void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPremitive);
    inline bool IsLeaf() const { return Children.empty(); }

    const FBoundingBox& GetCellBounds() const { return CellBounds; }
    const FBoundingBox& GetLooseBounds() const { return LooseBounds; }

    const TArray<FOctree*>& GetChildren() const { return Children; }

    TArray<UPrimitiveComponent*> FindNearestPrimitiveList(const FVector& Pos, const FVector& QueryExtent, uint32 Count);

    void QueryAABB(const FBoundingBox& QueryBox, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    void QueryFrustum(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    // Direct proxy output — skips Component→GetSceneProxy() indirection
    void QueryFrustumProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies) const;
    void QueryRay(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const;

    void Reset(const FBoundingBox& InBounds, uint32 InDepth = 0);

    FOctree* GetParent() const { return Parent; }

private:
    void SubDivide();
    void TryMerge();
    void ClearChildrenAndPrimitiveLocations();

    bool HasDistributable() const;

    // Collect all visible primitives in this node and descendants (no frustum test)
    void CollectAll(TArray<UPrimitiveComponent*>& OutPrimitives) const;
    void CollectAllProxies(TArray<FPrimitiveSceneProxy*>& OutProxies) const;
    // Recursive frustum query with containment propagation
    void QueryFrustumInternal(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives, bool bParentContained) const;
    void QueryFrustumProxiesInternal(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies, bool bParentContained) const;
    FBoundingBox CellBounds;
    FBoundingBox LooseBounds;

    TArray<FOctree*> Children;
    TArray<UPrimitiveComponent*> PrimitiveList;

    uint32 Depth;
    FOctree* Parent = nullptr;
};
