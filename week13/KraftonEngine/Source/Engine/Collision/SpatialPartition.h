#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Engine/Core/EngineTypes.h"
#include "Engine/Render/Culling/ConvexVolume.h"
#include <memory>

class UPrimitiveComponent;
class FPrimitiveSceneProxy;
class FOctree;
class AActor;
struct FRay;

class FSpatialPartition
{
	public:
    void FlushPrimitive();
	void Reset(const FBoundingBox& RootBounds);

    void InsertActor(AActor* Actor);
    void RemoveActor(AActor* Actor);
    void UpdateActor(AActor* Actor);

    void QueryFrustumAllPrimitive(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    void QueryFrustumAllProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveSceneProxy*>& OutProxies) const;
    void QueryRayAllPrimitive(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    //void QueryAABB(const FBoundingBox& Box, TArray<UPrimitiveComponent*>& OutPrimitives) const;

    const FOctree* GetOctree() const { return Octree.get(); }

private:
    void InsertPrimitive(UPrimitiveComponent* Primitive);
    void RemovePrimitive(UPrimitiveComponent* Primitive);

private:
    std::unique_ptr<FOctree> Octree;
    TArray<UPrimitiveComponent*> OverflowPrimitives;
    TArray<AActor*> DirtyActors;
};

