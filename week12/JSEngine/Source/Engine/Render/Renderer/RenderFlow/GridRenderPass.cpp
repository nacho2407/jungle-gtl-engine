#include "GridRenderPass.h"
#include "Render/LineBatcher.h"
#include "Render/Scene/RenderBus.h"

bool FGridRenderPass::Initialize()
{
    return true;
}

bool FGridRenderPass::Release()
{
    return true;
}

bool FGridRenderPass::Begin(const FRenderPassContext* Context)
{
    if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        OutSRV = PrevPassSRV;
        OutRTV = PrevPassRTV;
        return true;
    }

    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FGridRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }

    if (Context->GridLineBatcher == nullptr)
    {
        return true;
    }

    Context->GridLineBatcher->Flush(Context->DeviceContext);
    return true;
}

bool FGridRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
