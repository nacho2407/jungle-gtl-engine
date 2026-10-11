#include "GammaCorrectionPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"

REGISTER_RENDER_PASS(FGammaCorrectionPass)

FGammaCorrectionPass::FGammaCorrectionPass()
{
	PassType = ERenderPass::GammaCorrection;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FGammaCorrectionPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.SceneColorCopyTexture || !Frame.ViewportRenderTexture || !Frame.SceneColorCopySRV)
	{
		return false;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* SceneColorSRV = Frame.SceneColorCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);

	Cache.bForceAll = true;
	return true;
}

void FGammaCorrectionPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Ctx.Device.GetDeviceContext()->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
}
