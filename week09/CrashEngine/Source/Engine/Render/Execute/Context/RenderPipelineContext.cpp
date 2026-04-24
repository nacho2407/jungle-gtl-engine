// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Context/RenderPipelineContext.h"

#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"

const FRenderPassPreset& FRenderPipelineContext::GetRenderPassPreset(ERenderPass Pass) const
{
    return RenderPassPresets[(uint32)Pass];
}

const FRenderPassDrawPreset& FRenderPipelineContext::GetRenderPassDrawPreset(ERenderPass Pass) const
{
    return GetRenderPassPreset(Pass).Draw;
}

ID3D11RenderTargetView* FRenderPipelineContext::GetViewportRTV() const
{
    return Targets ? Targets->ViewportRTV : nullptr;
}

ID3D11DepthStencilView* FRenderPipelineContext::GetViewportDSV() const
{
    return Targets ? Targets->ViewportDSV : nullptr;
}
