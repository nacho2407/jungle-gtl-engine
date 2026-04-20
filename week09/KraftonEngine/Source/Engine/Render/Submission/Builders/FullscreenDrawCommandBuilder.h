#pragma once

#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Pipelines/Registry/ViewModePassConfig.h"

struct FRenderPipelineContext;
class FDrawCommandList;

class FFullscreenDrawCommandBuilder
{
public:
    static void Build(
        ERenderPass Pass,
        FRenderPipelineContext& Context,
        FDrawCommandList& OutList,
        EViewModePostProcessVariant PostProcessVariant = EViewModePostProcessVariant::None);
};
