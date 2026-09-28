#include "EditorRenderPass.h"
#include "Render/LineBatcher.h"

bool FEditorRenderPass::Initialize()
{
    return true;
}

bool FEditorRenderPass::Release()
{
    return true;
}

bool FEditorRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FEditorRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (Context->EditorLineBatcher == nullptr)
    {
        return true;
    }

    Context->EditorLineBatcher->Flush(Context->DeviceContext);
    return true;
}

bool FEditorRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
