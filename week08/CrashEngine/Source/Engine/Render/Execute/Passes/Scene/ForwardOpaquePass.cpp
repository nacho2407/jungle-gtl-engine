#include "Render/Execute/Passes/Scene/ForwardOpaquePass.h"

#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Submission/Command/DrawCommandList.h"

void FForwardOpaquePass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (Context.StateCache)
    {
        Context.StateCache->bForceAll = true;
    }
}

void FForwardOpaquePass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11RenderTargetView* RTV = Context.GetViewportRTV();
    Context.Context->OMSetRenderTargets(1, &RTV, Context.GetViewportDSV());
}

void FForwardOpaquePass::BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy)
{
    DrawCommandBuild::BuildMeshDrawCommand(Proxy, ERenderPass::Opaque, Context, *Context.DrawCommandList);
}

void FForwardOpaquePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::Opaque);
}
