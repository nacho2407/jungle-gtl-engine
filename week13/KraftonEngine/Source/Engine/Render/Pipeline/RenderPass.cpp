#include "RenderPass.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/FrameContext.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/DrawCommandList.h"
#include "Render/Pipeline/Renderer.h"

// ============================================================
// FPreDepthPass — DSV-only 렌더링 (색 출력 없음, Early-Z용)
// ============================================================

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

// ============================================================
// FOpaquePass — Depth Copy + MRT + Light Culling → 불투명 지오메트리
// ============================================================

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
	if (Frame.RenderOptions.LightCullingMode != ELightCullingMode::Off && Ctx.Renderer)
	{
		DC->OMSetRenderTargets(0, nullptr, nullptr);

		if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Tile)
		{
			Ctx.Renderer->UnbindClusterCullingResources();
			Ctx.Renderer->UnbindTileCullingResources();

			Ctx.Renderer->GetTileBaseCulling().Dispatch(
				DC,
				Frame,
				Ctx.Renderer->GetFrameBuffer(),
				Ctx.Renderer->GetTileCullingResource(),
				Ctx.Renderer->GetLightBufferSRV(),
				Ctx.Renderer->GetNumLights(),
				static_cast<uint32>(Frame.ViewportWidth),
				static_cast<uint32>(Frame.ViewportHeight)
			);
		}
		else if (Frame.RenderOptions.LightCullingMode == ELightCullingMode::Cluster)
		{
			Ctx.Renderer->DispatchClusterCullingResources();
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
			Ctx.Renderer->BindTileCullingResources();
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

// ============================================================
// Simple Passes — 상태만 정의, Begin/End 없음
// ============================================================

FDecalPass::FDecalPass()
{
	PassType    = ERenderPass::Decal;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

FAdditiveDecalPass::FAdditiveDecalPass()
{
	PassType    = ERenderPass::AdditiveDecal;
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::Additive,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

FAlphaBlendPass::FAlphaBlendPass()
{
	PassType    = ERenderPass::AlphaBlend;
	RenderState = { EDepthStencilState::Default, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, true };
}

FSelectionMaskPass::FSelectionMaskPass()
{
	PassType    = ERenderPass::SelectionMask;
	RenderState = { EDepthStencilState::StencilWrite, EBlendState::NoColor,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

FEditorLinesPass::FEditorLinesPass()
{
	PassType    = ERenderPass::EditorLines;
	RenderState = { EDepthStencilState::Default, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, false };
}

FGizmoOuterPass::FGizmoOuterPass()
{
	PassType    = ERenderPass::GizmoOuter;
	RenderState = { EDepthStencilState::GizmoOutside, EBlendState::Opaque,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

FGizmoInnerPass::FGizmoInnerPass()
{
	PassType    = ERenderPass::GizmoInner;
	RenderState = { EDepthStencilState::GizmoInside, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

FOverlayFontPass::FOverlayFontPass()
{
	PassType    = ERenderPass::OverlayFont;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidBackCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

// ============================================================
// FPostProcessPass — Stencil Copy 후 풀스크린 패스
// ============================================================

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

// ============================================================
// FFXAAPass — SceneColor Copy 후 안티앨리어싱
// ============================================================

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
