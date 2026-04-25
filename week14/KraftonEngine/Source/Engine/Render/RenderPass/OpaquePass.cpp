#include "OpaquePass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FOpaquePass)
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"

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

	// --- Depth Copy + MRT 설정 ---
	if (Frame.DepthTexture && Frame.DepthCopyTexture)
	{
		DC->OMSetRenderTargets(0, nullptr, nullptr);
		DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

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

		ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
		DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);

		Cache.bForceAll = true;
	}

	// --- Light Culling Dispatch ---
	if (Frame.RenderOptions.LightCullingMode != ELightCullingMode::Off)
	{
		DC->OMSetRenderTargets(0, nullptr, nullptr);

		if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Tile)
		{
			Ctx.Resources.UnbindClusterCullingResources(Ctx.Device);
			Ctx.Resources.UnbindTileCullingBuffers(Ctx.Device);
			Ctx.Resources.DispatchTileCulling(DC, Frame);
		}
		else if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Cluster)
		{
			Ctx.Resources.DispatchClusterCulling(Ctx.Device);
		}

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

		if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Tile)
		{
			Ctx.Resources.BindTileCullingBuffers(Ctx.Device);
		}

		Cache.bForceAll = true;
	}
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
