#include "FontRenderPass.h"
#include "Core/ResourceManager.h"
#include "Render/FontBatcher.h"

bool FFontRenderPass::Initialize()
{
    return true;
}

bool FFontRenderPass::Release()
{
    return true;
}

bool FFontRenderPass::Begin(const FRenderPassContext* Context)
{
    ID3D11RenderTargetView* RTV = PrevPassRTV;
    ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
    Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);

    OutSRV = PrevPassSRV;
    OutRTV = PrevPassRTV;
    return true;
}

bool FFontRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (Context->FontBatcher == nullptr)
    {
        return true;
    }

    const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
    Context->FontBatcher->Flush(Context->DeviceContext, FontRes);
    return true;
}

bool FFontRenderPass::End(const FRenderPassContext* Context)
{
    return true;
}
