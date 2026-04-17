#include "D3DDevice.h"

#include <d3d11sdklayers.h>
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"
#include "Render/Renderer/RenderTarget/DepthStencilFactory.h"


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
	ReleaseFrameBuffer();

	ReportLiveObjects();
	ReleaseDeviceAndSwapChain();
}

void FD3DDevice::BeginFrame()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV.Get(), ClearColor);

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

void FD3DDevice::BeginViewportFrame(FRenderTargetSet& InRenderTargetSet)
{
    const float ClearMask[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    DeviceContext->ClearRenderTargetView(InRenderTargetSet.SelectionMaskRTV, ClearMask);
    DeviceContext->ClearDepthStencilView(InRenderTargetSet.DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (InRenderTargetSet.SceneColorRTV && InRenderTargetSet.SelectionMaskRTV && InRenderTargetSet.DepthStencilView)
    {
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneColorRTV, ClearColor);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneNormalRTV, ClearNormal);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneLightRTV, ClearColor);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneFogRTV, ClearColor);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneWorldPosRTV, ClearColor);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SceneFXAARTV, ClearColor);
        DeviceContext->ClearRenderTargetView(InRenderTargetSet.SelectionMaskRTV, ClearMask);
        DeviceContext->ClearDepthStencilView(InRenderTargetSet.DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
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
