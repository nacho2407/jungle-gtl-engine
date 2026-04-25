#pragma once

#include "Render/Execute/Registry/RenderPipelineType.h"

class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FRenderPipelineContext;
struct FSceneView;

// FRenderPipelineRunner는 렌더 영역의 핵심 동작을 담당합니다.
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
