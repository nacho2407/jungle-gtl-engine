#include "Render/Passes/Common/RenderPassContext.h"
#include "Render/View/SceneView.h"
#include "Render/View/ViewportRenderTargets.h"
#include "Render/Passes/Common/PassRenderState.h"

const FPassRenderState& FRenderPassContext::GetPassState(ERenderPass Pass) const
{
    return PassRenderStates[(uint32)Pass];
}

ID3D11RenderTargetView* FRenderPassContext::GetViewportRTV() const
{
    return Targets ? Targets->ViewportRTV : nullptr;
}

ID3D11DepthStencilView* FRenderPassContext::GetViewportDSV() const
{
    return Targets ? Targets->ViewportDSV : nullptr;
}
