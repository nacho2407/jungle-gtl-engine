#pragma once

#include "Render/Core/RenderPipeline.h"

class FRenderer;
class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FFrameContext;

class FRenderPipelineRunner
{
public:
    void ExecutePipeline(
        ERenderPipelineType Root,
        FRenderer& Renderer,
        const FFrameContext& Frame,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;

private:
    void ExecutePipelineRecursive(
        ERenderPipelineType Type,
        FRenderer& Renderer,
        const FFrameContext& Frame,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;
};
