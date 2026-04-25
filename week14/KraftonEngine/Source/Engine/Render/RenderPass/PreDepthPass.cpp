#include "PreDepthPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"

REGISTER_RENDER_PASS(FPreDepthPass)

FPreDepthPass::FPreDepthPass()
{
	PassType    = ERenderPass::PreDepth;
	RenderState = { EDepthStencilState::Default, EBlendState::NoColor,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

void FPreDepthPass::BeginPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	DC->OMSetRenderTargets(0, nullptr, Ctx.Cache.DSV);
	Ctx.Cache.bForceAll = true;
}

void FPreDepthPass::EndPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	const FFrameContext& Frame = Ctx.Frame;

	// Depth Copy — LightCulling CS가 DepthCopySRV를 읽으므로 여기서 준비
	if (Frame.DepthTexture && Frame.DepthCopyTexture)
	{
		DC->OMSetRenderTargets(0, nullptr, nullptr);
		DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

		ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
		DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);
	}

	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	Ctx.Cache.bForceAll = true;
}
