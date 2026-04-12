#include "D3DDevice.h"

#include <d3d11sdklayers.h>


void FD3DDevice::Create(HWND InHWindow)
{
	CreateDeviceAndSwapChain(InHWindow);
	CreateFrameBuffer();
	CreateRasterizerState();
	CreateDepthStencilBuffer();
	CreateBlendState();
}

void FD3DDevice::Release()
{
	if (DeviceContext)
	{
		DeviceContext->ClearState();
		DeviceContext->Flush();
	}

	ReleaseBlendState();
	ReleaseDepthStencilBuffer();
	ReleaseRasterizerState();
	ReleaseViewportRenderTargets();
	ReleaseFrameBuffer();

	ReportLiveObjects();
	ReleaseDeviceAndSwapChain();
}

void FD3DDevice::BeginFrame()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);
	const float ClearMask[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceContext->ClearRenderTargetView(SelectionMaskRTV.Get(), ClearMask);
	DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	if (ViewportSceneColorRTV && ViewportSelectionMaskRTV && ViewportDepthStencilView)
	{
		DeviceContext->ClearRenderTargetView(ViewportSceneColorRTV.Get(), ClearColor);
        DeviceContext->ClearRenderTargetView(ViewportSceneNormalRTV.Get(), ClearNormal);
        DeviceContext->ClearRenderTargetView(ViewportSceneLightRTV.Get(), ClearColor);
        DeviceContext->ClearRenderTargetView(ViewportSceneFogRTV.Get(), ClearColor);
        DeviceContext->ClearRenderTargetView(ViewportSceneWorldPosRTV.Get(), ClearColor);
        DeviceContext->ClearRenderTargetView(ViewportSceneFXAARTV.Get(), ClearColor);
		DeviceContext->ClearRenderTargetView(ViewportSelectionMaskRTV.Get(), ClearMask);
		DeviceContext->ClearDepthStencilView(ViewportDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);

	SetRasterizerState(ERasterizerState::SolidBackCull);
	SetDepthStencilState(EDepthStencilState::Default);
	SetBlendState(EBlendState::Opaque);

	ID3D11RenderTargetView* FrameRTV = FrameBufferRTV.Get();
	DeviceContext->OMSetRenderTargets(1, &FrameRTV, DepthStencilView.Get());
}


void FD3DDevice::EndFrame()
{
	UINT PresentFlags = bTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
	SwapChain->Present(0, PresentFlags);
}

void FD3DDevice::OnResizeViewport(int Width, int Height)
{
	if (!SwapChain) return;

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	ReleaseDepthStencilBuffer();

	SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainFlags);

	ViewportInfo.Width = static_cast<float>(Width);
	ViewportInfo.Height = static_cast<float>(Height);

	CreateFrameBuffer();
	CreateDepthStencilBuffer();

	// 상태 캐시 초기화 — 새로 생성된 state 객체가 BeginFrame에서 재적용되도록
	CurrentRasterizerState = static_cast<ERasterizerState>(-1);
	CurrentDepthStencilState = static_cast<EDepthStencilState>(-1);
	CurrentBlendState = static_cast<EBlendState>(-1);
}

void FD3DDevice::EnsureViewportRenderTargets(int Width, int Height)
{
	if (Width <= 0 || Height <= 0)
	{
		ReleaseViewportRenderTargets();
		return;
	}

	const uint32 SafeWidth = static_cast<uint32>(Width);
	const uint32 SafeHeight = static_cast<uint32>(Height);
	if (ViewportRenderTargetWidth == SafeWidth && ViewportRenderTargetHeight == SafeHeight &&
		ViewportSceneColorRTV && ViewportSelectionMaskRTV && ViewportDepthStencilView)
	{
		return;
	}

	ReleaseViewportRenderTargets();
	CreateViewportRenderTargets(SafeWidth, SafeHeight);
}


void FD3DDevice::SetSubViewport(int32 X, int32 Y, int32 Width, int32 Height)
{
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = static_cast<float>(X);
	vp.TopLeftY = static_cast<float>(Y);
	vp.Width    = static_cast<float>(Width);
	vp.Height   = static_cast<float>(Height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	DeviceContext->RSSetViewports(1, &vp);
}

ID3D11Device* FD3DDevice::GetDevice() const
{
	return Device.Get();
}

ID3D11DeviceContext* FD3DDevice::GetDeviceContext() const
{
	return DeviceContext.Get();
}

FRenderTargetSet FD3DDevice::GetBackBufferRenderTargets() const
{
	FRenderTargetSet Targets;
	Targets.SceneColorRTV = FrameBufferRTV.Get();
        
	Targets.SelectionMaskRTV = SelectionMaskRTV.Get();
	Targets.SelectionMaskSRV = SelectionMaskSRV.Get();
	Targets.DepthStencilView = DepthStencilView.Get();
	Targets.Width = ViewportInfo.Width;
	Targets.Height = ViewportInfo.Height;
	return Targets;
}

FRenderTargetSet FD3DDevice::GetViewportRenderTargets() const
{
	FRenderTargetSet Targets;
	Targets.SceneColorRTV = ViewportSceneColorRTV.Get();
	Targets.SceneColorSRV = ViewportSceneColorSRV.Get();

    Targets.SceneNormalRTV = ViewportSceneNormalRTV.Get();
    Targets.SceneNormalSRV = ViewportSceneNormalSRV.Get();

    Targets.SceneLightRTV = ViewportSceneLightRTV.Get();
    Targets.SceneLightSRV = ViewportSceneLightSRV.Get();

	Targets.SceneFogRTV = ViewportSceneFogRTV.Get();
    Targets.SceneFogSRV = ViewportSceneFogSRV.Get();

	Targets.SceneWorldPosRTV = ViewportSceneWorldPosRTV.Get();
    Targets.SceneWorldPosSRV = ViewportSceneWorldPosSRV.Get();

	Targets.SceneFXAARTV = ViewportSceneFXAARTV.Get();
    Targets.SceneFXAASRV = ViewportSceneFXAASRV.Get();

    Targets.SceneDepthSRV = ViewportDepthStencilSRV.Get();
	Targets.SelectionMaskRTV = ViewportSelectionMaskRTV.Get();
	Targets.SelectionMaskSRV = ViewportSelectionMaskSRV.Get();
	Targets.DepthStencilView = ViewportDepthStencilView.Get();
	Targets.Width = static_cast<float>(ViewportRenderTargetWidth);
	Targets.Height = static_cast<float>(ViewportRenderTargetHeight);
	return Targets;
}

void FD3DDevice::SetDepthStencilState(EDepthStencilState InState)
{
	if (CurrentDepthStencilState == InState) return;

	switch (InState)
	{
	case EDepthStencilState::Default:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateDefault.Get(), 0);
		break;
	case EDepthStencilState::DepthReadOnly:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateDepthReadOnly.Get(), 0);
		break;
	case EDepthStencilState::StencilWrite:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilWrite.Get(), 1);
		break;
	case EDepthStencilState::StencilWriteOnlyEqual:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateStencilMaskEqual.Get(), 1);
		break;
	case EDepthStencilState::GizmoInside:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateGizmoInside.Get(), 1);
		break;
	case EDepthStencilState::GizmoOutside:
		DeviceContext->OMSetDepthStencilState(DepthStencilStateGizmoOutside.Get(), 1);
		break;
	}

	CurrentDepthStencilState = InState;
}

void FD3DDevice::SetBlendState(EBlendState InState)
{
	if (CurrentBlendState == InState) return;
	const float BlendFactor[4] = { 0, 0, 0, 0 };
	CurrentBlendState = InState;

	switch (CurrentBlendState)
	{
	case EBlendState::Opaque:
		DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
		break;

	case EBlendState::AlphaBlend:
		DeviceContext->OMSetBlendState(BlendStateAlpha.Get(), BlendFactor, 0xffffffff);
		break;

	case EBlendState::NoColor:
		DeviceContext->OMSetBlendState(BlendStateNoColorWrite.Get(), BlendFactor, 0xFFFFFFFF);
		break;
	}
}

void FD3DDevice::SetRasterizerState(ERasterizerState InState)
{
	if (CurrentRasterizerState == InState) return;

	switch (InState)
	{
	case ERasterizerState::SolidBackCull:
		DeviceContext->RSSetState(RasterizerStateBackCull.Get());
		break;
	case ERasterizerState::SolidFrontCull:
		DeviceContext->RSSetState(RasterizerStateFrontCull.Get());
		break;
	case ERasterizerState::SolidNoCull:
		DeviceContext->RSSetState(RasterizerStateNoCull.Get());
		break;
	case ERasterizerState::WireFrame:
		DeviceContext->RSSetState(RasterizerStateWireFrame.Get());
		break;
	}

	CurrentRasterizerState = InState;
}

void FD3DDevice::CreateDeviceAndSwapChain(HWND InHWindow)
{
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.OutputWindow = InHWindow;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// Check tearing support for no-vsync with flip model
	TComPtr<IDXGIFactory5> Factory5;
	{
		TComPtr<IDXGIFactory1> Factory1;
		if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(Factory1.GetAddressOf()))))
		{
			if (SUCCEEDED(Factory1->QueryInterface(__uuidof(IDXGIFactory5), reinterpret_cast<void**>(Factory5.GetAddressOf()))))
			{
				Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
					&bTearingSupported, sizeof(bTearingSupported));
			}
		}
	}

	if (bTearingSupported)
	{
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

	SwapChainFlags = swapChainDesc.Flags;

	UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
	CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		CreateDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
		&swapChainDesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), nullptr,
		DeviceContext.GetAddressOf());

	SwapChain->GetDesc(&swapChainDesc);

	ViewportInfo = { 0, 0, float(swapChainDesc.BufferDesc.Width), float(swapChainDesc.BufferDesc.Height), 0, 1 };

#if defined(_DEBUG)
	if (Device)
	{
		Device->QueryInterface(__uuidof(ID3D11Debug),
			reinterpret_cast<void**>(DebugDevice.GetAddressOf()));
		if (!DebugDevice)
		{
			OutputDebugStringA("[D3D11] Debug layer is not available. Install Graphics Tools or ensure debug runtime is present.\n");
		}
	}
#endif
}

void FD3DDevice::ReleaseDeviceAndSwapChain()
{
	//	Flush first
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}

	SwapChain.Reset();
	Device.Reset();
	DeviceContext.Reset();
}

void FD3DDevice::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(FrameBuffer.ReleaseAndGetAddressOf()));

	CD3D11_RENDER_TARGET_VIEW_DESC frameBufferRTVDesc = {};
	frameBufferRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	frameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer.Get(), &frameBufferRTVDesc,
		FrameBufferRTV.ReleaseAndGetAddressOf());

	D3D11_TEXTURE2D_DESC selectionMaskDesc = {};
	selectionMaskDesc.Width = static_cast<uint32>(ViewportInfo.Width);
	selectionMaskDesc.Height = static_cast<uint32>(ViewportInfo.Height);
	selectionMaskDesc.MipLevels = 1;
	selectionMaskDesc.ArraySize = 1;
	selectionMaskDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskDesc.SampleDesc.Count = 1;
	selectionMaskDesc.Usage = D3D11_USAGE_DEFAULT;
	selectionMaskDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Device->CreateTexture2D(&selectionMaskDesc, nullptr,
		SelectionMaskBuffer.ReleaseAndGetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC selectionMaskRTVDesc = {};
	selectionMaskRTVDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	Device->CreateRenderTargetView(SelectionMaskBuffer.Get(), &selectionMaskRTVDesc,
		SelectionMaskRTV.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC selectionMaskSRVDesc = {};
	selectionMaskSRVDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	selectionMaskSRVDesc.Texture2D.MostDetailedMip = 0;
	selectionMaskSRVDesc.Texture2D.MipLevels = 1;
	Device->CreateShaderResourceView(SelectionMaskBuffer.Get(), &selectionMaskSRVDesc,
		SelectionMaskSRV.ReleaseAndGetAddressOf());
}

void FD3DDevice::ReleaseFrameBuffer()
{
	SelectionMaskSRV.Reset();
	SelectionMaskRTV.Reset();
	SelectionMaskBuffer.Reset();
	FrameBufferRTV.Reset();
	FrameBuffer.Reset();
}

void FD3DDevice::CreateViewportRenderTargets(uint32 Width, uint32 Height)
{
	ViewportRenderTargetWidth = Width;
	ViewportRenderTargetHeight = Height;

	D3D11_TEXTURE2D_DESC sceneColorDesc = {};
	sceneColorDesc.Width = Width;
	sceneColorDesc.Height = Height;
	sceneColorDesc.MipLevels = 1;
	sceneColorDesc.ArraySize = 1;
	sceneColorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sceneColorDesc.SampleDesc.Count = 1;
	sceneColorDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Device->CreateTexture2D(&sceneColorDesc, nullptr,
		ViewportSceneColorTexture.ReleaseAndGetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC sceneColorRTVDesc = {};
	sceneColorRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sceneColorRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	Device->CreateRenderTargetView(ViewportSceneColorTexture.Get(), &sceneColorRTVDesc,
		ViewportSceneColorRTV.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC sceneColorSRVDesc = {};
	sceneColorSRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sceneColorSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	sceneColorSRVDesc.Texture2D.MostDetailedMip = 0;
	sceneColorSRVDesc.Texture2D.MipLevels = 1;
	Device->CreateShaderResourceView(ViewportSceneColorTexture.Get(), &sceneColorSRVDesc,
		ViewportSceneColorSRV.ReleaseAndGetAddressOf());

	D3D11_TEXTURE2D_DESC sceneNormalDesc = {};
    sceneNormalDesc.Width = Width;
    sceneNormalDesc.Height = Height;
    sceneNormalDesc.MipLevels = 1;
    sceneNormalDesc.ArraySize = 1;
    sceneNormalDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // ⭐ 중요
    sceneNormalDesc.SampleDesc.Count = 1;
    sceneNormalDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneNormalDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Device->CreateTexture2D(&sceneNormalDesc, nullptr, ViewportSceneNormalTexture.ReleaseAndGetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC sceneNormalRTVDesc = {};
    sceneNormalRTVDesc.Format = sceneNormalDesc.Format;
    sceneNormalRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(ViewportSceneNormalTexture.Get(), &sceneNormalRTVDesc,
                                   ViewportSceneNormalRTV.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC sceneNormalSRVDesc = {};
    sceneNormalSRVDesc.Format = sceneNormalDesc.Format;
    sceneNormalSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sceneNormalSRVDesc.Texture2D.MostDetailedMip = 0;
    sceneNormalSRVDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportSceneNormalTexture.Get(), &sceneNormalSRVDesc,
                                     ViewportSceneNormalSRV.ReleaseAndGetAddressOf());

	D3D11_TEXTURE2D_DESC selectionMaskDesc = {};
	selectionMaskDesc.Width = Width;
	selectionMaskDesc.Height = Height;
	selectionMaskDesc.MipLevels = 1;
	selectionMaskDesc.ArraySize = 1;
	selectionMaskDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskDesc.SampleDesc.Count = 1;
	selectionMaskDesc.Usage = D3D11_USAGE_DEFAULT;
	selectionMaskDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	Device->CreateTexture2D(&selectionMaskDesc, nullptr,
		ViewportSelectionMaskTexture.ReleaseAndGetAddressOf());

	D3D11_RENDER_TARGET_VIEW_DESC selectionMaskRTVDesc = {};
	selectionMaskRTVDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	Device->CreateRenderTargetView(ViewportSelectionMaskTexture.Get(), &selectionMaskRTVDesc,
		ViewportSelectionMaskRTV.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC selectionMaskSRVDesc = {};
	selectionMaskSRVDesc.Format = DXGI_FORMAT_R8_UNORM;
	selectionMaskSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	selectionMaskSRVDesc.Texture2D.MostDetailedMip = 0;
	selectionMaskSRVDesc.Texture2D.MipLevels = 1;
	Device->CreateShaderResourceView(ViewportSelectionMaskTexture.Get(), &selectionMaskSRVDesc,
		ViewportSelectionMaskSRV.ReleaseAndGetAddressOf());

	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
    depthStencilDesc.Width = Width;
    depthStencilDesc.Height = Height;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    depthStencilDesc.CPUAccessFlags = 0;
    depthStencilDesc.MiscFlags = 0;

	Device->CreateTexture2D(&depthStencilDesc, nullptr, ViewportDepthStencilTexture.ReleaseAndGetAddressOf());

    // DSV: typeless texture -> D24_UNORM_S8_UINT view
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = 0;
    dsvDesc.Texture2D.MipSlice = 0;

    Device->CreateDepthStencilView(ViewportDepthStencilTexture.Get(), &dsvDesc,
                                        ViewportDepthStencilView.ReleaseAndGetAddressOf());

    // SRV: typeless texture -> R24_UNORM_X8_TYPELESS view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportDepthStencilTexture.Get(), &srvDesc,
                                            ViewportDepthStencilSRV.ReleaseAndGetAddressOf());
        
	D3D11_TEXTURE2D_DESC lightDesc = {};
    lightDesc.Width = Width;
    lightDesc.Height = Height;
    lightDesc.MipLevels = 1;
    lightDesc.ArraySize = 1;
    lightDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR lighting buffer
    lightDesc.SampleDesc.Count = 1;
    lightDesc.Usage = D3D11_USAGE_DEFAULT;
    lightDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    lightDesc.CPUAccessFlags = 0;
    lightDesc.MiscFlags = 0;

    Device->CreateTexture2D(&lightDesc, nullptr, &ViewportSceneLightTexture);

	D3D11_RENDER_TARGET_VIEW_DESC lightRtvDesc = {};
    lightRtvDesc.Format = lightDesc.Format;
    lightRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    lightRtvDesc.Texture2D.MipSlice = 0;

    Device->CreateRenderTargetView(ViewportSceneLightTexture.Get(), &lightRtvDesc, &ViewportSceneLightRTV);

	D3D11_SHADER_RESOURCE_VIEW_DESC lightSrvDesc = {};
    lightSrvDesc.Format = lightDesc.Format;
    lightSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    lightSrvDesc.Texture2D.MostDetailedMip = 0;
    lightSrvDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportSceneLightTexture.Get(), &lightSrvDesc, &ViewportSceneLightSRV);

	D3D11_TEXTURE2D_DESC fogDesc = {};
    fogDesc.Width = Width;
    fogDesc.Height = Height;
    fogDesc.MipLevels = 1;
    fogDesc.ArraySize = 1;
    fogDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR foging buffer
    fogDesc.SampleDesc.Count = 1;
    fogDesc.Usage = D3D11_USAGE_DEFAULT;
    fogDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    fogDesc.CPUAccessFlags = 0;
    fogDesc.MiscFlags = 0;

    Device->CreateTexture2D(&fogDesc, nullptr, &ViewportSceneFogTexture);

    D3D11_RENDER_TARGET_VIEW_DESC fogRtvDesc = {};
    fogRtvDesc.Format = fogDesc.Format;
    fogRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    fogRtvDesc.Texture2D.MipSlice = 0;

    Device->CreateRenderTargetView(ViewportSceneFogTexture.Get(), &fogRtvDesc, &ViewportSceneFogRTV);

    D3D11_SHADER_RESOURCE_VIEW_DESC fogSrvDesc = {};
    fogSrvDesc.Format = fogDesc.Format;
    fogSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    fogSrvDesc.Texture2D.MostDetailedMip = 0;
    fogSrvDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportSceneFogTexture.Get(), &fogSrvDesc, &ViewportSceneFogSRV);

	D3D11_TEXTURE2D_DESC worldPosDesc = {};
    worldPosDesc.Width = Width;
    worldPosDesc.Height = Height;
    worldPosDesc.MipLevels = 1;
    worldPosDesc.ArraySize = 1;
    // 핵심: 월드 좌표는 float 포맷
    worldPosDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    // worldPosDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 더 정확히 필요하면
    worldPosDesc.SampleDesc.Count = 1;
    worldPosDesc.Usage = D3D11_USAGE_DEFAULT;
    // RTV + SRV 둘 다 필요
    worldPosDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    Device->CreateTexture2D(&worldPosDesc, nullptr, ViewportSceneWorldPosTexture.ReleaseAndGetAddressOf());

    // RTV
    D3D11_RENDER_TARGET_VIEW_DESC worldPosRTVDesc = {};
    worldPosRTVDesc.Format = worldPosDesc.Format;
    worldPosRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(ViewportSceneWorldPosTexture.Get(), &worldPosRTVDesc,
                                   ViewportSceneWorldPosRTV.ReleaseAndGetAddressOf());

    // SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC worldPosSRVDesc = {};
    worldPosSRVDesc.Format = worldPosDesc.Format;
    worldPosSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    worldPosSRVDesc.Texture2D.MostDetailedMip = 0;
    worldPosSRVDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportSceneWorldPosTexture.Get(), &worldPosSRVDesc,
                                     ViewportSceneWorldPosSRV.ReleaseAndGetAddressOf());

	// FXAA Texture
    D3D11_TEXTURE2D_DESC fxaaDesc = {};
    fxaaDesc.Width = Width;
    fxaaDesc.Height = Height;
    fxaaDesc.MipLevels = 1;
    fxaaDesc.ArraySize = 1;
    fxaaDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    fxaaDesc.SampleDesc.Count = 1;
    fxaaDesc.Usage = D3D11_USAGE_DEFAULT;
    fxaaDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Device->CreateTexture2D(&fxaaDesc, nullptr, ViewportSceneFXAATexture.ReleaseAndGetAddressOf());

    // FXAA RTV
    D3D11_RENDER_TARGET_VIEW_DESC fxaaRTVDesc = {};
    fxaaRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    fxaaRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(ViewportSceneFXAATexture.Get(), &fxaaRTVDesc,
                                   ViewportSceneFXAARTV.ReleaseAndGetAddressOf());

    // FXAA SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC fxaaSRVDesc = {};
    fxaaSRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    fxaaSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    fxaaSRVDesc.Texture2D.MostDetailedMip = 0;
    fxaaSRVDesc.Texture2D.MipLevels = 1;

    Device->CreateShaderResourceView(ViewportSceneFXAATexture.Get(), &fxaaSRVDesc,
                                     ViewportSceneFXAASRV.ReleaseAndGetAddressOf());
}

void FD3DDevice::ReleaseViewportRenderTargets()
{
	ViewportSelectionMaskSRV.Reset();
	ViewportSelectionMaskRTV.Reset();
	ViewportSelectionMaskTexture.Reset();

	ViewportSceneColorSRV.Reset();
	ViewportSceneColorRTV.Reset();
	ViewportSceneColorTexture.Reset();
    
	ViewportSceneNormalRTV.Reset();
    ViewportSceneNormalSRV.Reset();
    ViewportSceneNormalTexture.Reset();

    ViewportSceneLightRTV.Reset();
    ViewportSceneLightSRV.Reset();
    ViewportSceneLightTexture.Reset();

    ViewportDepthStencilView.Reset();
    ViewportDepthStencilTexture.Reset();
    ViewportDepthStencilSRV.Reset();

	ViewportSceneFogTexture.Reset();
    ViewportSceneFogRTV.Reset();
    ViewportSceneFogSRV.Reset();

	ViewportSceneWorldPosRTV.Reset();
	ViewportSceneWorldPosSRV.Reset();
    ViewportSceneWorldPosTexture.Reset();

	ViewportSceneFXAARTV.Reset();
	ViewportSceneFXAASRV.Reset();
    ViewportSceneFXAATexture.Reset();

	ViewportRenderTargetWidth = 0;
	ViewportRenderTargetHeight = 0;
}

void FD3DDevice::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&rasterizerDesc,
		RasterizerStateBackCull.ReleaseAndGetAddressOf());

	D3D11_RASTERIZER_DESC frontCullDesc = {};
	frontCullDesc.FillMode = D3D11_FILL_SOLID;
	frontCullDesc.CullMode = D3D11_CULL_FRONT;

	Device->CreateRasterizerState(&frontCullDesc,
		RasterizerStateFrontCull.ReleaseAndGetAddressOf());

	D3D11_RASTERIZER_DESC noCullDesc = {};
	noCullDesc.FillMode = D3D11_FILL_SOLID;
	noCullDesc.CullMode = D3D11_CULL_NONE;

	Device->CreateRasterizerState(&noCullDesc,
		RasterizerStateNoCull.ReleaseAndGetAddressOf());

	D3D11_RASTERIZER_DESC wireFrameDesc = {};
	wireFrameDesc.FillMode = D3D11_FILL_WIREFRAME;
	wireFrameDesc.CullMode = D3D11_CULL_NONE;

	Device->CreateRasterizerState(&wireFrameDesc,
		RasterizerStateWireFrame.ReleaseAndGetAddressOf());

	CurrentRasterizerState = ERasterizerState::SolidBackCull;
}

void FD3DDevice::ReleaseRasterizerState()
{
	RasterizerStateBackCull.Reset();
	RasterizerStateFrontCull.Reset();
	RasterizerStateNoCull.Reset();
	RasterizerStateWireFrame.Reset();
}

void FD3DDevice::CreateDepthStencilBuffer()
{
	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
	depthStencilDesc.Width = static_cast<uint32>(ViewportInfo.Width);
	depthStencilDesc.Height = static_cast<uint32>(ViewportInfo.Height);
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Device->CreateTexture2D(&depthStencilDesc, nullptr,
		DepthStencilBuffer.ReleaseAndGetAddressOf());
	Device->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr,
		DepthStencilView.ReleaseAndGetAddressOf());

	//	Default
	D3D11_DEPTH_STENCIL_DESC depthStencilStateDefaultDesc = {};
	depthStencilStateDefaultDesc.DepthEnable = TRUE;
	depthStencilStateDefaultDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilStateDefaultDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilStateDefaultDesc.StencilEnable = FALSE;

	Device->CreateDepthStencilState(&depthStencilStateDefaultDesc,
		DepthStencilStateDefault.ReleaseAndGetAddressOf());

	//	Depth Read Only
	D3D11_DEPTH_STENCIL_DESC depthStencilStateDepthReadOnlyDesc = {};
	depthStencilStateDepthReadOnlyDesc.DepthEnable = TRUE;
	depthStencilStateDepthReadOnlyDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilStateDepthReadOnlyDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilStateDepthReadOnlyDesc.StencilEnable = FALSE;

	Device->CreateDepthStencilState(&depthStencilStateDepthReadOnlyDesc,
		DepthStencilStateDepthReadOnly.ReleaseAndGetAddressOf());

	// Stencil Write
	D3D11_DEPTH_STENCIL_DESC depthStencilStateStencilWriteDesc = {};
	depthStencilStateStencilWriteDesc.DepthEnable = TRUE;
	depthStencilStateStencilWriteDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilStateStencilWriteDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilStateStencilWriteDesc.StencilEnable = TRUE;
	depthStencilStateStencilWriteDesc.StencilReadMask = 0xFF;
	depthStencilStateStencilWriteDesc.StencilWriteMask = 0xFF;

	depthStencilStateStencilWriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilStateStencilWriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	depthStencilStateStencilWriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilStateStencilWriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	depthStencilStateStencilWriteDesc.BackFace = depthStencilStateStencilWriteDesc.FrontFace;

	Device->CreateDepthStencilState(&depthStencilStateStencilWriteDesc,
		DepthStencilStateStencilWrite.ReleaseAndGetAddressOf());

	{// Gizmo split by selected-object stencil mask (ref=1)
		D3D11_DEPTH_STENCIL_DESC gizmoInsideDesc = {};
		gizmoInsideDesc.DepthEnable = TRUE;
		gizmoInsideDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		gizmoInsideDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

		gizmoInsideDesc.StencilEnable = TRUE;
		gizmoInsideDesc.StencilReadMask = 0xFF;
		gizmoInsideDesc.StencilWriteMask = 0x00;

		gizmoInsideDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		gizmoInsideDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		gizmoInsideDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		gizmoInsideDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		gizmoInsideDesc.BackFace = gizmoInsideDesc.FrontFace;

		Device->CreateDepthStencilState(&gizmoInsideDesc,
			DepthStencilStateGizmoInside.ReleaseAndGetAddressOf());


		D3D11_DEPTH_STENCIL_DESC gizmoOutsideDesc = {};
		gizmoOutsideDesc.DepthEnable = TRUE;
		gizmoOutsideDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		gizmoOutsideDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

		gizmoOutsideDesc.StencilEnable = TRUE;
		gizmoOutsideDesc.StencilReadMask = 0xFF;
		gizmoOutsideDesc.StencilWriteMask = 0x00;

		gizmoOutsideDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		gizmoOutsideDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		gizmoOutsideDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		gizmoOutsideDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

		gizmoOutsideDesc.BackFace = gizmoOutsideDesc.FrontFace;

		Device->CreateDepthStencilState(&gizmoOutsideDesc,
			DepthStencilStateGizmoOutside.ReleaseAndGetAddressOf());
	}



	//Stencil Mask Equal
	D3D11_DEPTH_STENCIL_DESC maskDesc = {};
	maskDesc.DepthEnable = TRUE;
	maskDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	maskDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	maskDesc.StencilEnable = TRUE;
	maskDesc.StencilReadMask = 0xFF;
	maskDesc.StencilWriteMask = 0x00;

	maskDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	maskDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	maskDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	maskDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	maskDesc.BackFace = maskDesc.FrontFace;

	Device->CreateDepthStencilState(&maskDesc,
		DepthStencilStateStencilMaskEqual.ReleaseAndGetAddressOf());

}

void FD3DDevice::CreateBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	Device->CreateBlendState(&blendDesc, BlendStateAlpha.ReleaseAndGetAddressOf());

	//No Color
	D3D11_BLEND_DESC noColorWriteDesc = {};
	noColorWriteDesc.AlphaToCoverageEnable = FALSE;
	noColorWriteDesc.IndependentBlendEnable = FALSE;
	noColorWriteDesc.RenderTarget[0].BlendEnable = FALSE;
	noColorWriteDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	noColorWriteDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	noColorWriteDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	noColorWriteDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	noColorWriteDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	noColorWriteDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	noColorWriteDesc.RenderTarget[0].RenderTargetWriteMask = 0;

	Device->CreateBlendState(&noColorWriteDesc, BlendStateNoColorWrite.ReleaseAndGetAddressOf());
}

void FD3DDevice::ReleaseDepthStencilBuffer()
{
	DepthStencilStateDefault.Reset();
	DepthStencilStateDepthReadOnly.Reset();
	DepthStencilStateStencilWrite.Reset();
	DepthStencilStateStencilMaskEqual.Reset();
	DepthStencilStateGizmoInside.Reset();
	DepthStencilStateGizmoOutside.Reset();
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
}

void FD3DDevice::ReleaseBlendState()
{
	BlendStateAlpha.Reset();
	BlendStateNoColorWrite.Reset();
}

void FD3DDevice::ReportLiveObjects()
{
#if defined(_DEBUG)
	if (DebugDevice)
	{
		DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		DebugDevice.Reset();
	}
#endif
}
