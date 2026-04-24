// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Execute/Registry/RenderPassTypes.h"
#include "Render/Execute/Passes/Scene/DepthPrePass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"

void FDepthPrePass::PrepareInputs(FRenderPipelineContext& Context)
{
    ID3D11ShaderResourceView* NullSRVs[6] = {};
    Context.Context->PSSetShaderResources(0, ARRAY_SIZE(NullSRVs), NullSRVs);
}

void FDepthPrePass::PrepareTargets(FRenderPipelineContext& Context)
{
    ID3D11DepthStencilView* DSV = Context.GetViewportDSV();
    Context.Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
    Context.Context->OMSetRenderTargets(0, nullptr, DSV);
}

void FDepthPrePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::DepthPre);
}

