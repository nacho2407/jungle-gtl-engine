#include "SubUVRenderPass.h"
#include "Render/SubUVBatcher.h"
#include "Render/Scene/RenderBus.h"

namespace
{
    bool IsDebugViewMode(EViewMode ViewMode)
    {
        return ViewMode == EViewMode::Normal ||
            ViewMode == EViewMode::Heatmap ||
            ViewMode == EViewMode::BoneWeightHeatmap ||
            ViewMode == EViewMode::Depth;
    }
}

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

bool FSubUVRenderPass::DrawCommand(const FRenderPassContext* Context)
{
    if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
    {
        return true;
    }

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
