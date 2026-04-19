#include "Render/Renderer/RenderPipelineRunner.h"

#include "Render/Renderer/RenderPassRegistry.h"
#include "Render/Renderer/RenderPipelineRegistry.h"
#include "Render/Renderer/Renderer.h"

void FRenderPipelineRunner::ExecutePipeline(
    ERenderPipelineType Root,
    FRenderer& Renderer,
    const FFrameContext& Frame,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    ExecutePipelineRecursive(Root, Renderer, Frame, PipelineRegistry, PassRegistry);
}

void FRenderPipelineRunner::ExecutePipelineRecursive(
    ERenderPipelineType Type,
    FRenderer& Renderer,
    const FFrameContext& Frame,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    const FRenderPipelineDesc* Desc = PipelineRegistry.FindPipeline(Type);
    if (!Desc)
    {
        return;
    }

    for (const FRenderNodeRef& Child : Desc->Children)
    {
        if (Child.Kind == ERenderNodeKind::Pipeline)
        {
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, Renderer, Frame, PipelineRegistry, PassRegistry);
        }
        else
        {
            if (FRenderPass* Pass = PassRegistry.FindPass((ERenderPassNodeType)Child.TypeValue))
            {
                Pass->Execute(Renderer, Frame);
            }
        }
    }
}
