#include "Render/Renderer/RenderPipelineRunner.h"

#include "Render/Core/RenderPassContext.h"
#include "Render/Renderer/RenderPassRegistry.h"
#include "Render/Renderer/RenderPipelineRegistry.h"

void FRenderPipelineRunner::ExecutePipeline(
    ERenderPipelineType Root,
    FRenderPassContext& Context,
    const FFrameContext& Frame,
    const FRenderPipelineRegistry& PipelineRegistry,
    const FRenderPassRegistry& PassRegistry) const
{
    ExecutePipelineRecursive(Root, Context, Frame, PipelineRegistry, PassRegistry);
}

void FRenderPipelineRunner::ExecutePipelineRecursive(
    ERenderPipelineType Type,
    FRenderPassContext& Context,
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
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, Context, Frame, PipelineRegistry, PassRegistry);
        }
        else if (FRenderPass* Pass = PassRegistry.FindPass((ERenderPassNodeType)Child.TypeValue))
        {
            Pass->Execute(Context, Frame);
        }
    }
}
