#include "Render/Passes/Scene/DepthPrePass.h"
#include "Render/Core/RenderPassContext.h"
#include "Render/Commands/DrawCommandList.h"
#include "Render/Scene/PrimitiveSceneProxy.h"

void FDepthPrePass::PrepareInputs(FRenderPassContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
}

void FDepthPrePass::PrepareTargets(FRenderPassContext& Context)
{
    ID3D11DepthStencilView* DSV = Context.GetViewportDSV();
    Context.Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
    Context.Context->OMSetRenderTargets(0, nullptr, DSV);
}

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context)
{
    (void)Context;
}

void FDepthPrePass::BuildDrawCommands(FRenderPassContext& Context, const FPrimitiveSceneProxy& Proxy)
{
    (void)Context;
    (void)Proxy;
}

void FDepthPrePass::SubmitDrawCommands(FRenderPassContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0, End = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::DepthPre, Start, End);
    if (Start < End)
    {
        Context.DrawCommandList->SubmitRange(Start, End, *Context.Device, Context.Context, *Context.StateCache);
    }
}
