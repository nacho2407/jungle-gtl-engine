#include "Render/Pipelines/Context/RenderPipelineContext.h"
#include "Render/Pipelines/Context/View/ViewportRenderTargets.h"
#include "Render/Passes/Base/PassRenderState.h"

const FPassRenderState& FRenderPipelineContext::GetPassState(ERenderPass Pass) const
{
    return PassRenderStates[(uint32)Pass];
}

ID3D11RenderTargetView* FRenderPipelineContext::GetViewportRTV() const
{
    return Targets ? Targets->ViewportRTV : nullptr;
}

ID3D11DepthStencilView* FRenderPipelineContext::GetViewportDSV() const
{
    return Targets ? Targets->ViewportDSV : nullptr;
}
