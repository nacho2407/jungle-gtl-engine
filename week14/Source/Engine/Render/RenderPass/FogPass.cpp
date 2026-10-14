#include "Render/RenderPass/FogPass.h"
#include "Render/RenderPass/RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommandList.h"

REGISTER_RENDER_PASS(FFogPass)

FFogPass::FFogPass()
{
	PassType    = ERenderPass::Fog;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FFogPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture) return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	Cache.bForceAll = true;
	return true;
}
