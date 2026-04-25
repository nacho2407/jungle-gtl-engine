#include "FXAAPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FFXAAPass)
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"

FFXAAPass::FFXAAPass()
{
	PassType    = ERenderPass::FXAA;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

void FFXAAPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.SceneColorCopyTexture || !Frame.ViewportRenderTexture)
		return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);

	Cache.bForceAll = true;
}
