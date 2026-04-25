#include "OpaquePass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FOpaquePass)
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"

FOpaquePass::FOpaquePass()
{
	PassType    = ERenderPass::Opaque;
	RenderState = { EDepthStencilState::DepthGreaterEqual, EBlendState::Opaque,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

void FOpaquePass::BeginPass(const FPassContext& Ctx)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	const FFrameContext& Frame = Ctx.Frame;
	FStateCache& Cache = Ctx.Cache;

	// MRT 설정 (NormalRTV, CullingHeatmapRTV)
	if (Frame.NormalRTV)
	{
		ID3D11RenderTargetView* RTVs[3] = { Cache.RTV, Frame.NormalRTV, Frame.CullingHeatmapRTV };
		uint32 NumRTs = Frame.CullingHeatmapRTV ? 3 : 2;
		DC->OMSetRenderTargets(NumRTs, RTVs, Cache.DSV);
	}
	else
	{
		DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
	}

	Cache.bForceAll = true;
}

void FOpaquePass::EndPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.NormalRTV) return;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	if (Frame.NormalSRV)
	{
		ID3D11ShaderResourceView* normalSRV = Frame.NormalSRV;
		DC->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &normalSRV);
	}
	if (Frame.CullingHeatmapSRV)
	{
		ID3D11ShaderResourceView* heatmapSRV = Frame.CullingHeatmapSRV;
		DC->PSSetShaderResources(ESystemTexSlot::CullingHeatmap, 1, &heatmapSRV);
	}

	Cache.bForceAll = true;
}
