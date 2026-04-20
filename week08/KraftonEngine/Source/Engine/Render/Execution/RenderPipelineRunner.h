#pragma once

#include "Render/Pipelines/Registry/RenderPipelineType.h"

class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FRenderPassContext;
struct FSceneView; using FFrameContext = FSceneView;

class FRenderPipelineRunner
{
public:
    void ExecutePipeline(
        ERenderPipelineType Root,
        FRenderPassContext& Context,
        const FFrameContext& Frame,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;

private:
    void ExecutePipelineRecursive(
        ERenderPipelineType Type,
        FRenderPassContext& Context,
        const FFrameContext& Frame,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;
};
