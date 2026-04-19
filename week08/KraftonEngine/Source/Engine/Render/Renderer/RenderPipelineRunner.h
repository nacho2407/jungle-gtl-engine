#pragma once

class FRenderPipelineRegistry;
class FRenderPassRegistry;
class FRenderPassContext;
struct FFrameContext;

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
