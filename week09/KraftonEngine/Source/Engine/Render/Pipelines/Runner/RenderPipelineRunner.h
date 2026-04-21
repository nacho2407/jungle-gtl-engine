#pragma once

#include "Render/Pipelines/Registry/RenderPipelineType.h"

class FRenderPipelineRegistry;
class FRenderPassRegistry;
struct FRenderPipelineContext;
struct FSceneView;

/*
    파이프라인 레지스트리를 따라 패스/서브파이프라인을 재귀적으로 실행하는 실행기입니다.
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
