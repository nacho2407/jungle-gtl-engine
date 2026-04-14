#include "D3DDevice.h"

#include <d3d11sdklayers.h>
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"


void FD3DDevice::Create(HWND InHWindow)
{
	CreateDeviceAndSwapChain(InHWindow);
	CreateFrameBuffer();
	CreateDepthStencilBuffer();
}

void FD3DDevice::Release()
{
	if (DeviceContext)
	{
		DeviceContext->ClearState();
		DeviceContext->Flush();
	}

	ReleaseDepthStencilBuffer();
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
    FRenderTarget RT;
	ViewportRenderTargetWidth = Width;
	ViewportRenderTargetHeight = Height;

	RT = FRenderTargetFactory::CreateSceneColor(GetDevice(), Width, Height);

	ViewportSceneColorTexture = RT.Texture;
        ViewportSceneColorRTV = RT.RTV;
        ViewportSceneColorSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneNormal(GetDevice(), Width, Height);

	ViewportSceneNormalTexture = RT.Texture;
        ViewportSceneNormalRTV = RT.RTV;
        ViewportSceneNormalSRV = RT.SRV;


	RT = FRenderTargetFactory::CreateSelectionMask(GetDevice(), Width, Height);
        ViewportSelectionMaskTexture = RT.Texture;
        ViewportSelectionMaskRTV = RT.RTV;
        ViewportSelectionMaskSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneLight(GetDevice(), Width, Height);
    ViewportSceneLightTexture = RT.Texture;
    ViewportSceneLightRTV = RT.RTV;
    ViewportSceneLightSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneFog(GetDevice(), Width, Height);
    ViewportSceneFogTexture = RT.Texture;
    ViewportSceneFogRTV = RT.RTV;
    ViewportSceneFogSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneWorldPos(GetDevice(), Width, Height);
    ViewportSceneWorldPosTexture = RT.Texture;
    ViewportSceneWorldPosRTV = RT.RTV;
    ViewportSceneWorldPosSRV = RT.SRV;

	RT = FRenderTargetFactory::CreateSceneFXAA(GetDevice(), Width, Height);
    ViewportSceneFXAATexture = RT.Texture;
    ViewportSceneFXAARTV = RT.RTV;
    ViewportSceneFXAASRV = RT.SRV;

	
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
}

void FD3DDevice::ReleaseDepthStencilBuffer()
{
	DepthStencilView.Reset();
	DepthStencilBuffer.Reset();
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
