#pragma once

#include "Render/Common/ViewTypes.h"

class FMeshBufferManager;
class FRenderBus;
class UPrimitiveComponent;

class FPrimitiveDrawCommandBuilder
{
public:
    bool CollectPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                          FRenderBus& RenderBus, FMeshBufferManager& MeshBufferManager) const;
};
