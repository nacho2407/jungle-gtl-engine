#include "FogPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
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
	if (!Ctx.Frame.RenderOptions.ShowFlags.bFog)
		return false;

	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.DepthCopySRV)
		return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	// t16 SceneDepth SRV null unbind — CopyResource 전 read/write hazard 방지
	ID3D11ShaderResourceView* NullSRV = nullptr;
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);

	// RT/DSV unbind → depth 복사 → RT/DSV 복구
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	// DepthCopySRV를 t16에 재바인딩
	ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);

	Cache.bForceAll = true;
	return true;
}
