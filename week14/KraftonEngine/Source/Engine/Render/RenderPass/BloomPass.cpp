#include "BloomPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"
REGISTER_RENDER_PASS(FBloomPass)

namespace
{
	struct FBloomBlurConstants
	{
		FVector2 TexelSize;
		FVector2 Direction;
		float Radius = 1.0f;
		float Pad[3] = {};
	};

	void DrawFullscreenTriangle(ID3D11DeviceContext* DC)
	{
		DC->IASetInputLayout(nullptr);
		DC->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DC->Draw(3, 0);
	}
}

FBloomPass::FBloomPass()
{
	PassType = ERenderPass::Bloom;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
					ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FBloomPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	return Frame.RenderOptions.ShowFlags.bBloom
		&& Frame.SceneColorCopyTexture
		&& Frame.ViewportRenderTexture
		&& Frame.SceneColorCopySRV
		&& Frame.BloomRTVA
		&& Frame.BloomSRVA
		&& Frame.BloomRTVB
		&& Frame.BloomSRVB;
}

void FBloomPass::Execute(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	if (!DC)
	{
		return;
	}

	FShader* ExtractShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomExtract);
	FShader* BlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomBlur);
	FShader* CompositeShader = FShaderManager::Get().GetOrCreate(EShaderPath::BloomComposite);
	if (!ExtractShader || !BlurShader || !CompositeShader)
	{
		return;
	}

	if (!BloomBlurCB.GetBuffer())
	{
		BloomBlurCB.Create(Ctx.Device.GetDevice(), sizeof(FBloomBlurConstants), "BloomBlurCB");
	}

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11ShaderResourceView* NullSystemSRVs[11] = {};
	D3D11_VIEWPORT BloomViewport = {};
	BloomViewport.Width = Frame.BloomWidth;
	BloomViewport.Height = Frame.BloomHeight;
	BloomViewport.MinDepth = 0.0f;
	BloomViewport.MaxDepth = 1.0f;

	D3D11_VIEWPORT SceneViewport = {};
	SceneViewport.Width = Frame.ViewportWidth;
	SceneViewport.Height = Frame.ViewportHeight;
	SceneViewport.MinDepth = 0.0f;
	SceneViewport.MaxDepth = 1.0f;

	// Keep the original HDR scene color stable while ping-ponging bloom into A/B.
	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);

	// 1. Extract bright pixels: SceneColorCopy -> BloomA.
	DC->RSSetViewports(1, &BloomViewport);
	DC->OMSetRenderTargets(1, &Frame.BloomRTVA, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &Frame.SceneColorCopySRV);
	ExtractShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);

	const float InvWidth = Frame.BloomWidth > 0.0f ? 1.0f / Frame.BloomWidth : 0.0f;
	const float InvHeight = Frame.BloomHeight > 0.0f ? 1.0f / Frame.BloomHeight : 0.0f;

	ID3D11Buffer* BlurCBRaw = BloomBlurCB.GetBuffer();
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &BlurCBRaw);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &BlurCBRaw);

	// 2. Horizontal blur: BloomA -> BloomB.
	FBloomBlurConstants BlurData = {};
	BlurData.TexelSize = FVector2(InvWidth, InvHeight);
	BlurData.Radius = 1.35f;
	BlurData.Direction = FVector2(1.0f, 0.0f);
	BloomBlurCB.Update(DC, &BlurData, sizeof(BlurData));

	DC->OMSetRenderTargets(1, &Frame.BloomRTVB, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &Frame.BloomSRVA);
	BlurShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);

	// 3. Vertical blur: BloomB -> BloomA.
	BlurData.Direction = FVector2(0.0f, 1.0f);
	BloomBlurCB.Update(DC, &BlurData, sizeof(BlurData));

	DC->OMSetRenderTargets(1, &Frame.BloomRTVA, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &Frame.BloomSRVB);
	BlurShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);

	// 4. Second horizontal blur: BloomA -> BloomB.
	BlurData.Direction = FVector2(1.0f, 0.0f);
	BloomBlurCB.Update(DC, &BlurData, sizeof(BlurData));

	DC->OMSetRenderTargets(1, &Frame.BloomRTVB, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &Frame.BloomSRVA);
	BlurShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);

	// 5. Second vertical blur: BloomB -> BloomA.
	BlurData.Direction = FVector2(0.0f, 1.0f);
	BloomBlurCB.Update(DC, &BlurData, sizeof(BlurData));

	DC->OMSetRenderTargets(1, &Frame.BloomRTVA, nullptr);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &Frame.BloomSRVB);
	BlurShader->Bind(DC);
	DrawFullscreenTriangle(DC);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);

	// 6. Composite: SceneColorCopy + BloomA -> Viewport scene color.
	DC->RSSetViewports(1, &SceneViewport);
	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &Frame.SceneColorCopySRV);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &Frame.BloomSRVA);
	CompositeShader->Bind(DC);
	DrawFullscreenTriangle(DC);

	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);
	DC->PSSetShaderResources(0, ARRAYSIZE(NullSystemSRVs), NullSystemSRVs);
	ID3D11Buffer* NullCB = nullptr;
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &NullCB);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &NullCB);

	Ctx.Cache.bForceAll = true;
}

void FBloomPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Ctx.Device.GetDeviceContext()->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
	Ctx.Device.GetDeviceContext()->PSSetShaderResources(ESystemTexSlot::Bloom, 1, &NullSRV);
}
