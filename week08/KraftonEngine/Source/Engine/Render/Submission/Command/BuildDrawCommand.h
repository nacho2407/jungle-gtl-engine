#pragma once

#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

class FPrimitiveSceneProxy;
class FTextRenderSceneProxy;
class FDrawCommandList;
struct FRenderPipelineContext;

namespace DrawCommandBuild
{
void BuildMeshDrawCommand(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildFullscreenDrawCommand(ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList, EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None);

void BuildLineDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildOverlayBillboardDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildOverlayTextDrawCommand(FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
void BuildOverlayWorldTextDrawCommand(const FTextRenderSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);

void BuildDecalDrawCommand(const FPrimitiveSceneProxy& Proxy, FRenderPipelineContext& Context, FDrawCommandList& OutList);
} // namespace DrawCommandBuild
