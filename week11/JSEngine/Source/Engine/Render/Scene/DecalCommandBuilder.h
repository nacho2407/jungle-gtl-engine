#pragma once

#include "Core/CoreMinimal.h"
#include "Spatial/WorldSpatialIndex.h"

class FMeshBufferManager;
class FRenderBus;
class UPrimitiveComponent;
struct FShowFlags;

struct FRenderDecalStats
{
    int32 TotalDecalCount = 0;
    int32 CollectTimeMS = 0;
};

class FDecalCommandBuilder
{
public:
    void Reset();
    void CollectDecal(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                      FMeshBufferManager& MeshBufferManager,
                      FWorldSpatialIndex::FPrimitiveOBBQueryScratch& OBBQueryScratch);

    const FRenderDecalStats& GetLastStats() const { return LastStats; }

private:
    FRenderDecalStats LastStats;
};
