// 충돌/피킹 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Engine/Core/EngineTypes.h"
#include "Render/Visibility/Frustum/ConvexVolume.h"
#include <memory>

class UPrimitiveComponent;
class FPrimitiveProxy;
class FOctree;
class AActor;
struct FRay;

// FSpatialPartition 클래스이다.
class FSpatialPartition
{
public:
    void FlushPrimitive();
    void Reset(const FBoundingBox& RootBounds);

    void InsertActor(AActor* Actor);
    void RemoveActor(AActor* Actor);
    void UpdateActor(AActor* Actor);
    void AddSinglePrimitive(UPrimitiveComponent* Primitive);

    // 단일 컴포넌트 제거 ? overflow / octree leaf 양쪽 모두 정리한다.
    void RemoveSinglePrimitive(UPrimitiveComponent* Primitive);

    void QueryFrustumAllPrimitive(const FConvexVolume& ConvexVolume, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    void QueryFrustumAllProxies(const FConvexVolume& ConvexVolume, TArray<FPrimitiveProxy*>& OutProxies) const;
    void QueryRayAllPrimitive(const FRay& Ray, TArray<UPrimitiveComponent*>& OutPrimitives) const;
    // void QueryAABB(const FBoundingBox& Box, TArray<UPrimitiveComponent*>& OutPrimitives) const;

    const FOctree* GetOctree() const { return Octree.get(); }

private:
    FBoundingBox BuildActorVisibleBounds(AActor* Actor, bool bUpdateWorldMatrices) const;
    void EnsureRootContains(const FBoundingBox& RequiredBounds);
    void RebuildRootBounds(const FBoundingBox& RequiredBounds);
    void ClearQueuedActorFlags();
    void InsertPrimitive(UPrimitiveComponent* Primitive);
    void RemovePrimitive(UPrimitiveComponent* Primitive);

private:
    std::unique_ptr<FOctree> Octree;
    TArray<UPrimitiveComponent*> OverflowPrimitives;
    TArray<AActor*> DirtyActors;
};

