#include "GridRenderPass.h"
#include "Render/LineBatcher.h"

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
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FGridRenderPass::DrawCommand(const FRenderPassContext* Context)
{
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
