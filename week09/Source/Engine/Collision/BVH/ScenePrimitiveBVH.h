#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Collision/BVH/BVH.h"

class AActor;
class UPrimitiveComponent;
class UStaticMeshComponent;

class FScenePrimitiveBVH
{
public:
    static constexpr int32 ChildFanout = 8;
    static constexpr int32 LeafPacketSize = 8;
    static constexpr int32 MaxLeafSize = 16;

    void MarkDirty();
    void BuildNow(const TArray<AActor*>& Actors);
    void EnsureBuilt(const TArray<AActor*>& Actors);
    bool Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const;
    bool IsDirty() const { return bDirty; }

    struct FLeaf
    {
        FBoundingBox Bounds;
        UPrimitiveComponent* Primitive = nullptr;
        UStaticMeshComponent* StaticMeshPrimitive = nullptr;
        AActor* Owner = nullptr;
    };

    using FBVH = TBVH<FLeaf, ChildFanout, MaxLeafSize>;
    using FNode = FBVH::FNode;

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

    const TArray<FLeaf>& GetLeaves() const { return Leaves; }
    const TArray<FNode>& GetNodes() const { return BVH.GetNodes(); }

private:
    bool bDirty = true;
    TArray<FLeaf> Leaves;
    FBVH BVH;
    TArray<FPrimitivePacket> PrimitivePackets;
};
