#include "SubUVRenderPass.h"
#include "Render/SubUVBatcher.h"
#include "Render/Scene/RenderBus.h"

bool FSubUVRenderPass::Initialize()
{
    return true;
}

bool FSubUVRenderPass::Release()
{
    return true;
}

bool FSubUVRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FSubUVRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (Context->SubUVBatcher == nullptr)
    {
        return true;
    }

    const bool bWireframe = (Context->RenderBus != nullptr) && (Context->RenderBus->GetViewMode() == EViewMode::Wireframe);
    Context->SubUVBatcher->Flush(Context->DeviceContext, bWireframe);
    return true;
}

bool FSubUVRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
