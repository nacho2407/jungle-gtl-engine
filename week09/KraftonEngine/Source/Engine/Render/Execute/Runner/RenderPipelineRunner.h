#pragma once

#include "Render/Execute/Registry/RenderPipelineType.h"

class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FRenderPipelineContext;
struct FSceneView;

/*
    ?�이?�라???��??�트리�? ?�라 ?�스/?�브?�이?�라?�을 ?��??�으�??�행?�는 ?�행기입?�다.
*/
class FRenderPipelineRunner
{
public:
    void ExecutePipeline(
        ERenderPipelineType Root,
        FRenderPipelineContext& Context,
        const FSceneView& SceneView,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;

private:
    void ExecutePipelineRecursive(
        ERenderPipelineType Type,
        FRenderPipelineContext& Context,
        const FSceneView& SceneView,
        const FRenderPipelineRegistry& PipelineRegistry,
        const FRenderPassRegistry& PassRegistry) const;
};
