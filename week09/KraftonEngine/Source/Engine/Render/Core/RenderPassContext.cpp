#include "Render/Core/RenderPassContext.h"
#include "Render/Core/FrameContext.h"
#include "Render/Core/PassRenderState.h"

const FPassRenderState& FRenderPassContext::GetPassState(ERenderPass Pass) const
{
    return PassRenderStates[(uint32)Pass];
}

ID3D11RenderTargetView* FRenderPassContext::GetViewportRTV() const
{
    return Frame ? Frame->ViewportRTV : nullptr;
}

ID3D11DepthStencilView* FRenderPassContext::GetViewportDSV() const
{
    return Frame ? Frame->ViewportDSV : nullptr;
}
