#pragma once

#include "Render/Execute/Registry/RenderPipelineType.h"

class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FRenderPipelineContext;
struct FSceneView;

class FRenderPipelineRunner
{
public:
    void ExecutePipeline(
        ERenderPipelineType            Root,
        FRenderPipelineContext&        Context,
        const FSceneView&              SceneView,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry&     PassRegistry) const;

private:
    void ExecutePipelineRecursive(
        ERenderPipelineType            Type,
        FRenderPipelineContext&        Context,
        const FSceneView&              SceneView,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry&     PassRegistry) const;
};
