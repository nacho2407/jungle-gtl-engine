#include "PostProcessPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FPostProcessPass)
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"

FPostProcessPass::FPostProcessPass()
{
	PassType    = ERenderPass::PostProcess;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

void FPostProcessPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.StencilCopySRV)
		return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);

	Cache.bForceAll = true;
}
