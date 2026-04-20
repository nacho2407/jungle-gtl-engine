#pragma once

#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"

class FPrimitiveSceneProxy;
struct FRenderPipelineContext;
class FDrawCommandList;

class FMeshDrawCommandBuilder
{
public:
    static void Build(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass, FRenderPipelineContext& Context, FDrawCommandList& OutList);
};
