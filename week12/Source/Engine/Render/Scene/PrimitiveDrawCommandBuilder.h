#pragma once

#include "Render/Common/ViewTypes.h"

class FRenderResourceProvider;
class FRenderBus;
class UPrimitiveComponent;

class FPrimitiveDrawCommandBuilder
{
public:
    bool CollectPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                          FRenderBus& RenderBus, FRenderResourceProvider& ResourceProvider) const;
    bool CollectShadowCasterPrimitive(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                      FRenderBus& RenderBus, FRenderResourceProvider& ResourceProvider) const;

private:
    bool CollectPrimitiveInternal(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode,
                                  FRenderBus& RenderBus, FRenderResourceProvider& ResourceProvider, bool bShadowOnly) const;
};
