#include "EarlyPostProcessPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"

#include <algorithm>
#include <cmath>

REGISTER_RENDER_PASS(FEarlyPostProcessPass)

FEarlyPostProcessPass::FEarlyPostProcessPass()
{
	PassType    = ERenderPass::EarlyPostProcess;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FEarlyPostProcessPass::BeginPass(const FPassContext& Ctx)
{
	const bool bHasEarlyCommands = Ctx.CommandList.GetCommandCount(ERenderPass::EarlyPostProcess) > 0;
	const bool bDOF = Ctx.Frame.RenderOptions.ShowFlags.bDepthOfField;
	if (!bHasEarlyCommands && !bDOF)
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

void FEarlyPostProcessPass::Execute(const FPassContext& Ctx)
{
	FRenderPassBase::Execute(Ctx);

	if (Ctx.Frame.RenderOptions.ShowFlags.bDepthOfField)
	{
		ExecuteDepthOfField(Ctx);
	}
}

void FEarlyPostProcessPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11ShaderResourceView* NullSRVs[3] = {};
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	DC->VSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
}

void FEarlyPostProcessPass::EnsureResources(const FPassContext& Ctx)
{
	if (bResourcesCreated)
		return;

	DOFConstantBuffer.Create(Ctx.Device.GetDevice(), sizeof(FDepthOfFieldConstants), "DepthOfFieldCB");
	bResourcesCreated = true;
}

void FEarlyPostProcessPass::ExecuteDepthOfField(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.SceneColorCopyTexture || !Frame.ViewportRenderTexture || !Frame.SceneColorCopySRV
		|| !Frame.DOFColorCoCRTV || !Frame.DOFColorCoCSRV
		|| !Frame.DOFPrefilterRTV || !Frame.DOFPrefilterSRV
		|| !Frame.DOFFarBlurRTV || !Frame.DOFFarBlurSRV
		|| !Frame.DOFNearBlurRTV || !Frame.DOFNearBlurSRV
		|| !Frame.DOFBokehRTV || !Frame.DOFBokehSRV)
	{
		return;
	}

	EnsureResources(Ctx);

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	FDepthOfFieldConstants Constants = {};
	const float FullWidth = std::max(Frame.ViewportWidth, 1.0f);
	const float FullHeight = std::max(Frame.ViewportHeight, 1.0f);
	const float HalfWidth = std::max(std::floor(FullWidth * 0.5f), 1.0f);
	const float HalfHeight = std::max(std::floor(FullHeight * 0.5f), 1.0f);
	Constants.Params0 = FVector4(
		std::max(Frame.RenderOptions.DOFFocalDistance, Frame.NearClip),
		std::max(Frame.RenderOptions.DOFAperture, 0.0f),
		std::max(Frame.RenderOptions.DOFMaxCoCRadius, 0.0f),
		Frame.NearClip);
	Constants.Params1 = FVector4(Frame.FarClip, 1.0f / FullWidth, 1.0f / FullHeight, 1.0f / HalfWidth);
	Constants.Params2 = FVector4(
		1.0f / HalfHeight,
		static_cast<float>(std::clamp(Frame.RenderOptions.DOFApertureBladeCount, 3, 16)),
		std::max(Frame.RenderOptions.DOFBokehThreshold, 0.0f),
		std::max(Frame.RenderOptions.DOFBokehIntensity, 0.0f));
	Constants.Params3 = FVector4(
		std::max(Frame.RenderOptions.DOFBokehRadiusScale, 0.0f),
		0.0f,
		0.0f,
		0.0f);
	DOFConstantBuffer.Update(DC, &Constants, sizeof(Constants));

	ID3D11Buffer* RawDOFCB = DOFConstantBuffer.GetBuffer();
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawDOFCB);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawDOFCB);

	ID3D11ShaderResourceView* NullSRVs[3] = {};
	ID3D11ShaderResourceView* NullSystemSRV = nullptr;

	DC->VSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSystemSRV);
	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);

	ID3D11ShaderResourceView* SceneColorSRV = Frame.SceneColorCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DC->IASetInputLayout(nullptr);
	DC->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

	D3D11_VIEWPORT FullViewport = {};
	FullViewport.Width = FullWidth;
	FullViewport.Height = FullHeight;
	FullViewport.MinDepth = 0.0f;
	FullViewport.MaxDepth = 1.0f;

	D3D11_VIEWPORT HalfViewport = FullViewport;
	HalfViewport.Width = HalfWidth;
	HalfViewport.Height = HalfHeight;

	auto DrawFullscreen = [&](FShader* Shader, ID3D11RenderTargetView* RTV, const D3D11_VIEWPORT& Viewport)
	{
		if (!Shader || !Shader->IsValid() || !RTV)
			return;

		DC->OMSetRenderTargets(1, &RTV, nullptr);
		DC->RSSetViewports(1, &Viewport);
		Shader->Bind(DC);
		DC->Draw(3, 0);
	};

	FShader* DownsampleShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFDownSampling);
	FShader* PrefilterShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFCoCPrefilter);
	FShader* FarBlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFBlurFar);
	FShader* NearBlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFBlurNear);
	FShader* BokehShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFBokeh);
	FShader* RecombineShader = FShaderManager::Get().GetOrCreate(EShaderPath::DOFRecombine);

	DrawFullscreen(DownsampleShader, Frame.DOFColorCoCRTV, HalfViewport);

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSystemSRV);
	ID3D11ShaderResourceView* ColorCoCSRV = Frame.DOFColorCoCSRV;
	DC->PSSetShaderResources(0, 1, &ColorCoCSRV);
	DrawFullscreen(PrefilterShader, Frame.DOFPrefilterRTV, HalfViewport);

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	ID3D11ShaderResourceView* PrefilterSRV = Frame.DOFPrefilterSRV;
	DC->PSSetShaderResources(0, 1, &PrefilterSRV);
	DrawFullscreen(FarBlurShader, Frame.DOFFarBlurRTV, HalfViewport);

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(0, 1, &PrefilterSRV);
	DrawFullscreen(NearBlurShader, Frame.DOFNearBlurRTV, HalfViewport);

	const float ClearBokeh[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DC->ClearRenderTargetView(Frame.DOFBokehRTV, ClearBokeh);
	if (Frame.RenderOptions.ShowFlags.bDOFBokeh)
	{
		DC->OMSetRenderTargets(1, &Frame.DOFBokehRTV, nullptr);
		DC->RSSetViewports(1, &HalfViewport);
		Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Additive);
		DC->VSSetShaderResources(0, 3, NullSRVs);
		DC->PSSetShaderResources(0, 3, NullSRVs);
		DC->VSSetShaderResources(0, 1, &PrefilterSRV);
		DC->PSSetShaderResources(0, 1, &PrefilterSRV);
		if (BokehShader && BokehShader->IsValid())
		{
			BokehShader->Bind(DC);
			const uint32 HalfPixelCount = static_cast<uint32>(HalfWidth * HalfHeight);
			DC->Draw(HalfPixelCount * 6u, 0);
		}
		Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	}

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->VSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
	ID3D11ShaderResourceView* RecombineSRVs[3] = { Frame.DOFFarBlurSRV, Frame.DOFNearBlurSRV, Frame.DOFBokehSRV };
	DC->PSSetShaderResources(0, 3, RecombineSRVs);
	DrawFullscreen(RecombineShader, Cache.RTV, FullViewport);

	DC->VSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(0, 3, NullSRVs);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSystemSRV);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
	DC->RSSetViewports(1, &FullViewport);

	Cache.bForceAll = true;
}
