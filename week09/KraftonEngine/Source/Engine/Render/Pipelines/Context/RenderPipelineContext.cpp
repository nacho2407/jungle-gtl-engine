#include "Render/Pipelines/Context/RenderPipelineContext.h"

#include "Render/Pipelines/Context/Viewport/ViewportRenderTargets.h"

const FPassRenderStateDesc& FRenderPipelineContext::GetPassState(ERenderPass Pass) const
{
    return PassStateDescs[(uint32)Pass];
}

ID3D11RenderTargetView* FRenderPipelineContext::GetViewportRTV() const
{
    return Targets ? Targets->ViewportRTV : nullptr;
}

ID3D11DepthStencilView* FRenderPipelineContext::GetViewportDSV() const
{
    return Targets ? Targets->ViewportDSV : nullptr;
}
