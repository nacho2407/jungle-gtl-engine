#include "Viewport/Viewport.h"

#include "Render/Resource/Buffer.h"

namespace
{
	constexpr DXGI_FORMAT SceneColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
}

FViewport::~FViewport()
{
	ReleaseResources();
}

bool FViewport::Initialize(ID3D11Device* InDevice, uint32 InWidth, uint32 InHeight)
{
	Device = InDevice;
	Width = InWidth;
	Height = InHeight;

	return CreateResources();
}

void FViewport::Release()
{
	ReleaseResources();
	Device = nullptr;
	Width = 0;
	Height = 0;
}

void FViewport::Resize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0) return;
	if (InWidth == Width && InHeight == Height) return;

	Width = InWidth;
	Height = InHeight;

	ReleaseResources();
	CreateResources();
}

void FViewport::RequestResize(uint32 InWidth, uint32 InHeight)
{
	if (InWidth == 0 || InHeight == 0) return;
	if (InWidth == Width && InHeight == Height)
	{
		bPendingResize = false;
		return;
	}

	PendingWidth = InWidth;
	PendingHeight = InHeight;
	bPendingResize = true;
}

bool FViewport::ApplyPendingResize()
{
	if (!bPendingResize) return false;

	bPendingResize = false;
	Resize(PendingWidth, PendingHeight);
	return true;
}

void FViewport::BeginRender(ID3D11DeviceContext* Ctx, const float ClearColor[4])
{
	if (!RTV) return;

	const float DefaultColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
	const float* Color = ClearColor ? ClearColor : DefaultColor;
	D3D11_VIEWPORT VPRect = GetViewportRect();

	Ctx->ClearRenderTargetView(RTV, Color);
	if (NormalRTV)
	{
		const float NormalClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		Ctx->ClearRenderTargetView(NormalRTV, NormalClear);
	}
	if (CullingHeatmapRTV)
	{
		const float HeatmapClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		Ctx->ClearRenderTargetView(CullingHeatmapRTV, HeatmapClear);
	}
	Ctx->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
	Ctx->OMSetRenderTargets(1, &RTV, DSV);
	Ctx->RSSetViewports(1, &VPRect);
}

bool FViewport::CreateResources()
{
	if (!Device || Width == 0 || Height == 0) return false;

	// ── 렌더 타깃 텍스처 ──
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Width;
	TexDesc.Height = Height;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = SceneColorFormat;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &RTTexture);
	if (FAILED(hr)) return false;
	RTTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorTexture")), "ViewportSceneColorTexture");

	hr = Device->CreateRenderTargetView(RTTexture, nullptr, &RTV);
	if (FAILED(hr)) return false;
	RTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorRTV")), "ViewportSceneColorRTV");

	hr = Device->CreateShaderResourceView(RTTexture, nullptr, &SRV);
	if (FAILED(hr)) return false;
	SRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorSRV")), "ViewportSceneColorSRV");

	// ── SceneColor 복사 텍스처 (FXAA 등 PostProcess용 CopyResource 대상) ──
	D3D11_TEXTURE2D_DESC SceneColorCopyDesc = TexDesc;
	SceneColorCopyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // SRV 읽기 전용
	hr = Device->CreateTexture2D(&SceneColorCopyDesc, nullptr, &SceneColorCopyTexture);
	if (FAILED(hr)) return false;
	SceneColorCopyTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorCopyTexture")), "ViewportSceneColorCopyTexture");

	// ── 뎁스/스텐실 (TYPELESS → DSV + StencilSRV) ──
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthTexture);
	if (FAILED(hr)) return false;
	DepthTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthTexture")), "ViewportDepthTexture");

	// DSV: D24_UNORM_S8_UINT 로 해석 (기존과 동일한 뎁스/스텐실 동작)
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateDepthStencilView(DepthTexture, &DSVDesc, &DSV);
	if (FAILED(hr)) return false;
	DSV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDSV")), "ViewportDSV");

	// SRV 포맷 (DepthCopy/StencilCopy 생성에 재사용)
	D3D11_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc = {};
	DepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	DepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	DepthSRVDesc.Texture2D.MipLevels = 1;
	DepthSRVDesc.Texture2D.MostDetailedMip = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC StencilSRVDesc = {};
	StencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	StencilSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	StencilSRVDesc.Texture2D.MipLevels = 1;
	StencilSRVDesc.Texture2D.MostDetailedMip = 0;

	// ── Depth 복사 텍스처 (CopyResource 대상, SRV 전용) ──
	D3D11_TEXTURE2D_DESC CopyDesc = {};
	CopyDesc.Width = Width;
	CopyDesc.Height = Height;
	CopyDesc.MipLevels = 1;
	CopyDesc.ArraySize = 1;
	CopyDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	CopyDesc.SampleDesc.Count = 1;
	CopyDesc.Usage = D3D11_USAGE_DEFAULT;
	CopyDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&CopyDesc, nullptr, &DepthCopyTexture);
	if (FAILED(hr)) return false;
	DepthCopyTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthCopyTexture")), "ViewportDepthCopyTexture");

	hr = Device->CreateShaderResourceView(DepthCopyTexture, &DepthSRVDesc, &DepthCopySRV);
	if (FAILED(hr)) return false;
	DepthCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDepthCopySRV")), "ViewportDepthCopySRV");

	hr = Device->CreateShaderResourceView(DepthCopyTexture, &StencilSRVDesc, &StencilCopySRV);
	if (FAILED(hr)) return false;
	StencilCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportStencilCopySRV")), "ViewportStencilCopySRV");

	D3D11_SHADER_RESOURCE_VIEW_DESC SceneColorCopySRVDesc = {};
	SceneColorCopySRVDesc.Format = SceneColorFormat;
	SceneColorCopySRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SceneColorCopySRVDesc.Texture2D.MipLevels = 1;
	SceneColorCopySRVDesc.Texture2D.MostDetailedMip = 0;

	hr = Device->CreateShaderResourceView(SceneColorCopyTexture, &SceneColorCopySRVDesc, &SceneColorCopySRV);
	if (FAILED(hr)) return false;
	SceneColorCopySRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportSceneColorCopySRV")), "ViewportSceneColorCopySRV");

	// ── DOF half-resolution intermediates (RGB=color, A=CoC) ──
	D3D11_TEXTURE2D_DESC DOFDesc = {};
	DOFDesc.Width = Width > 1 ? Width / 2 : 1;
	DOFDesc.Height = Height > 1 ? Height / 2 : 1;
	DOFDesc.MipLevels = 1;
	DOFDesc.ArraySize = 1;
	DOFDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DOFDesc.SampleDesc.Count = 1;
	DOFDesc.Usage = D3D11_USAGE_DEFAULT;
	DOFDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&DOFDesc, nullptr, &DOFColorCoCTexture);
	if (FAILED(hr)) return false;
	DOFColorCoCTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFColorCoCTexture")), "ViewportDOFColorCoCTexture");
	hr = Device->CreateRenderTargetView(DOFColorCoCTexture, nullptr, &DOFColorCoCRTV);
	if (FAILED(hr)) return false;
	DOFColorCoCRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFColorCoCRTV")), "ViewportDOFColorCoCRTV");
	hr = Device->CreateShaderResourceView(DOFColorCoCTexture, nullptr, &DOFColorCoCSRV);
	if (FAILED(hr)) return false;
	DOFColorCoCSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFColorCoCSRV")), "ViewportDOFColorCoCSRV");

	hr = Device->CreateTexture2D(&DOFDesc, nullptr, &DOFBlurTexture);
	if (FAILED(hr)) return false;
	DOFBlurTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBlurTexture")), "ViewportDOFBlurTexture");
	hr = Device->CreateRenderTargetView(DOFBlurTexture, nullptr, &DOFBlurRTV);
	if (FAILED(hr)) return false;
	DOFBlurRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBlurRTV")), "ViewportDOFBlurRTV");
	hr = Device->CreateShaderResourceView(DOFBlurTexture, nullptr, &DOFBlurSRV);
	if (FAILED(hr)) return false;
	DOFBlurSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBlurSRV")), "ViewportDOFBlurSRV");

	hr = Device->CreateTexture2D(&DOFDesc, nullptr, &DOFBokehTexture);
	if (FAILED(hr)) return false;
	DOFBokehTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBokehTexture")), "ViewportDOFBokehTexture");
	hr = Device->CreateRenderTargetView(DOFBokehTexture, nullptr, &DOFBokehRTV);
	if (FAILED(hr)) return false;
	DOFBokehRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBokehRTV")), "ViewportDOFBokehRTV");
	hr = Device->CreateShaderResourceView(DOFBokehTexture, nullptr, &DOFBokehSRV);
	if (FAILED(hr)) return false;
	DOFBokehSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportDOFBokehSRV")), "ViewportDOFBokehSRV");

	// ── GBuffer Normal RT (R16G16B16A16_FLOAT — 음수 지원) ──
	D3D11_TEXTURE2D_DESC NormalDesc = {};
	NormalDesc.Width = Width;
	NormalDesc.Height = Height;
	NormalDesc.MipLevels = 1;
	NormalDesc.ArraySize = 1;
	NormalDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	NormalDesc.SampleDesc.Count = 1;
	NormalDesc.Usage = D3D11_USAGE_DEFAULT;
	NormalDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&NormalDesc, nullptr, &NormalTexture);
	if (FAILED(hr)) return false;
	NormalTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalTexture")), "ViewportNormalTexture");

	hr = Device->CreateRenderTargetView(NormalTexture, nullptr, &NormalRTV);
	if (FAILED(hr)) return false;
	NormalRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalRTV")), "ViewportNormalRTV");

	hr = Device->CreateShaderResourceView(NormalTexture, nullptr, &NormalSRV);
	if (FAILED(hr)) return false;
	NormalSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportNormalSRV")), "ViewportNormalSRV");

	// ── Culling Heatmap RT (R8G8B8A8_UNORM — 히트맵 색상) ──
	D3D11_TEXTURE2D_DESC HeatmapDesc = {};
	HeatmapDesc.Width = Width;
	HeatmapDesc.Height = Height;
	HeatmapDesc.MipLevels = 1;
	HeatmapDesc.ArraySize = 1;
	HeatmapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	HeatmapDesc.SampleDesc.Count = 1;
	HeatmapDesc.Usage = D3D11_USAGE_DEFAULT;
	HeatmapDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	hr = Device->CreateTexture2D(&HeatmapDesc, nullptr, &CullingHeatmapTexture);
	if (FAILED(hr)) return false;
	CullingHeatmapTexture->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapTexture")), "ViewportCullingHeatmapTexture");

	hr = Device->CreateRenderTargetView(CullingHeatmapTexture, nullptr, &CullingHeatmapRTV);
	if (FAILED(hr)) return false;
	CullingHeatmapRTV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapRTV")), "ViewportCullingHeatmapRTV");

	hr = Device->CreateShaderResourceView(CullingHeatmapTexture, nullptr, &CullingHeatmapSRV);
	if (FAILED(hr)) return false;
	CullingHeatmapSRV->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen("ViewportCullingHeatmapSRV")), "ViewportCullingHeatmapSRV");

	// ── 뷰포트 렉트 ──
	ViewportRect.TopLeftX = 0.0f;
	ViewportRect.TopLeftY = 0.0f;
	ViewportRect.Width = static_cast<float>(Width);
	ViewportRect.Height = static_cast<float>(Height);
	ViewportRect.MinDepth = 0.0f;
	ViewportRect.MaxDepth = 1.0f;

	return true;
}

void FViewport::ReleaseResources()
{
	if (DOFBokehSRV) { DOFBokehSRV->Release(); DOFBokehSRV = nullptr; }
	if (DOFBokehRTV) { DOFBokehRTV->Release(); DOFBokehRTV = nullptr; }
	if (DOFBokehTexture) { DOFBokehTexture->Release(); DOFBokehTexture = nullptr; }
	if (DOFBlurSRV) { DOFBlurSRV->Release(); DOFBlurSRV = nullptr; }
	if (DOFBlurRTV) { DOFBlurRTV->Release(); DOFBlurRTV = nullptr; }
	if (DOFBlurTexture) { DOFBlurTexture->Release(); DOFBlurTexture = nullptr; }
	if (DOFColorCoCSRV) { DOFColorCoCSRV->Release(); DOFColorCoCSRV = nullptr; }
	if (DOFColorCoCRTV) { DOFColorCoCRTV->Release(); DOFColorCoCRTV = nullptr; }
	if (DOFColorCoCTexture) { DOFColorCoCTexture->Release(); DOFColorCoCTexture = nullptr; }
	if (CullingHeatmapSRV) { CullingHeatmapSRV->Release(); CullingHeatmapSRV = nullptr; }
	if (CullingHeatmapRTV) { CullingHeatmapRTV->Release(); CullingHeatmapRTV = nullptr; }
	if (CullingHeatmapTexture) { CullingHeatmapTexture->Release(); CullingHeatmapTexture = nullptr; }
	if (NormalSRV) { NormalSRV->Release(); NormalSRV = nullptr; }
	if (NormalRTV) { NormalRTV->Release(); NormalRTV = nullptr; }
	if (NormalTexture) { NormalTexture->Release(); NormalTexture = nullptr; }
	if (StencilCopySRV) { StencilCopySRV->Release(); StencilCopySRV = nullptr; }
	if (DepthCopySRV) { DepthCopySRV->Release(); DepthCopySRV = nullptr; }
	if (DepthCopyTexture) { DepthCopyTexture->Release(); DepthCopyTexture = nullptr; }
	if (DSV) { DSV->Release(); DSV = nullptr; }
	if (DepthTexture) { DepthTexture->Release(); DepthTexture = nullptr; }
	if (SRV) { SRV->Release(); SRV = nullptr; }
	if (RTV) { RTV->Release(); RTV = nullptr; }
	if (RTTexture) { RTTexture->Release(); RTTexture = nullptr; }
	if (SceneColorCopySRV) { SceneColorCopySRV->Release(); SceneColorCopySRV = nullptr; }
	if (SceneColorCopyTexture) { SceneColorCopyTexture->Release(); SceneColorCopyTexture = nullptr; }
}
