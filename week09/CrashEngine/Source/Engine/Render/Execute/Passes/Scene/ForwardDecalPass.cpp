#include "Render/Execute/Passes/Scene/ForwardDecalPass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FForwardDecalPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (Context.StateCache)
    {
        Context.StateCache->bForceAll = true;
    }
}

void FForwardDecalPass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FForwardDecalPass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildDecalDrawCommand(Proxy, Context, *Context.DrawCommandList);
}

void FForwardDecalPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Decal);
}
