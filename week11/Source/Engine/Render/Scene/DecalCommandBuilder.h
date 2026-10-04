#pragma once

#include "Core/CoreMinimal.h"
#include "Spatial/WorldSpatialIndex.h"

class FMeshBufferManager;
class FRenderBus;
class UPrimitiveComponent;
struct FShowFlags;

class FDecalCommandBuilder
{
public:
    void CollectDecal(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
                      FMeshBufferManager& MeshBufferManager,
                      FWorldSpatialIndex::FPrimitiveOBBQueryScratch& OBBQueryScratch);
};
