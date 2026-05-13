#include "EditorOverlayRenderPass.h"
#include "Render/LineBatcher.h"

bool FEditorOverlayRenderPass::Initialize()
{
    return true;
}

bool FEditorOverlayRenderPass::Release()
{
    return true;
}

bool FEditorOverlayRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FEditorOverlayRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (Context->EditorOverlayLineBatcher == nullptr)
    {
        return true;
    }

    Context->EditorOverlayLineBatcher->Flush(Context->DeviceContext);
    return true;
}

bool FEditorOverlayRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
