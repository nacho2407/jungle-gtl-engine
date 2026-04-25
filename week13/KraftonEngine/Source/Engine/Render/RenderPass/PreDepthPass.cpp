#include "PreDepthPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
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
	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	Ctx.Cache.bForceAll = true;
}
