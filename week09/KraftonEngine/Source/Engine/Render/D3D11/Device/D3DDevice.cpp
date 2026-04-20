#include "Render/D3D11/Device/D3DDevice.h"

//	Safe Release Macro
#define SAFE_RELEASE(Obj) if (Obj) { Obj->Release(); Obj = nullptr; }

void FD3DDevice::Create(HWND InHWindow)
{
	CreateDeviceAndSwapChain(InHWindow);
	CreateFrameBuffer();
	RasterizerStateManager.Create(Device);
	CreateDepthStencilBuffer();
	DepthStencilStateManager.Create(Device);
	BlendStateManager.Create(Device);
}

void FD3DDevice::Release()
{
	DeviceContext->ClearState();
	DeviceContext->Flush();

	BlendStateManager.Release();
	DepthStencilStateManager.Release();
	ReleaseDepthStencilBuffer();
	RasterizerStateManager.Release();
	ReleaseFrameBuffer();

	ReleaseDeviceAndSwapChain();
}

void FD3DDevice::Present()
{
	UINT PresentFlags = bTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0;
	SwapChain->Present(0, PresentFlags);
}

void FD3DDevice::OnResizeViewport(int Width, int Height)
{
	if (!SwapChain) return;

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	DepthStencilStateManager.Release();
	ReleaseDepthStencilBuffer();

	SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, SwapChainFlags);

	ViewportInfo.Width = static_cast<float>(Width);
	ViewportInfo.Height = static_cast<float>(Height);

	CreateFrameBuffer();
	CreateDepthStencilBuffer();
	DepthStencilStateManager.Create(Device);

	// 상태 캐시 초기화 — 새로 생성된 state 객체가 BeginFrame에서 재적용되도록
	RasterizerStateManager.ResetCache();
	DepthStencilStateManager.ResetCache();
	BlendStateManager.ResetCache();
}

ID3D11Device* FD3DDevice::GetDevice() const
{
	return Device;
}

ID3D11DeviceContext* FD3DDevice::GetDeviceContext() const
{
	return DeviceContext;
}

void FD3DDevice::SetDepthStencilState(EDepthStencilState InState)
{
	DepthStencilStateManager.Set(DeviceContext, InState);
}

void FD3DDevice::SetBlendState(EBlendState InState)
{
	BlendStateManager.Set(DeviceContext, InState);
}

void FD3DDevice::SetRasterizerState(ERasterizerState InState)
{
	RasterizerStateManager.Set(DeviceContext, InState);
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
	IDXGIFactory5* Factory5 = nullptr;
	{
		IDXGIFactory1* Factory1 = nullptr;
		if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&Factory1)))
		{
			if (SUCCEEDED(Factory1->QueryInterface(__uuidof(IDXGIFactory5), (void**)&Factory5)))
			{
				Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
					&bTearingSupported, sizeof(bTearingSupported));
			}
			Factory1->Release();
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
		&swapChainDesc, &SwapChain, &Device, nullptr, &DeviceContext);

	if (DeviceContext)
	{
		DeviceContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)&UserDefinedAnnotation);
	}

	// CPU가 GPU보다 1프레임 이상 앞서지 못하게 제한
	// (기본값 3 → Present 큐 깊이로 인한 FPS 톱니파 현상 방지)
	{
		IDXGIDevice1* DXGIDevice = nullptr;
		if (SUCCEEDED(Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&DXGIDevice)))
		{
			DXGIDevice->SetMaximumFrameLatency(1);
			DXGIDevice->Release();
		}
	}

	if (Factory5) Factory5->Release();

	SwapChain->GetDesc(&swapChainDesc);

	ViewportInfo = { 0, 0, float(swapChainDesc.BufferDesc.Width), float(swapChainDesc.BufferDesc.Height), 0, 1 };
}

void FD3DDevice::ReleaseDeviceAndSwapChain()
{
	SAFE_RELEASE(UserDefinedAnnotation);
	//	Flush first
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}

	SAFE_RELEASE(SwapChain);
	SAFE_RELEASE(DeviceContext);

#ifdef _DEBUG
	// 릭 진단: Device 해제 직전 남아있는 D3D 객체 리포트
	if (Device)
	{
		ID3D11Debug* Debug = nullptr;
		if (SUCCEEDED(Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&Debug)))
		{
			OutputDebugStringA("=== ReportLiveDeviceObjects BEGIN ===\n");
			Debug->ReportLiveDeviceObjects(
				static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL));
			OutputDebugStringA("=== ReportLiveDeviceObjects END ===\n");
			Debug->Release();
		}
		else
		{
			OutputDebugStringA("=== ID3D11Debug QueryInterface FAILED ===\n");
		}
	}
#endif

	SAFE_RELEASE(Device);
}

void FD3DDevice::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	CD3D11_RENDER_TARGET_VIEW_DESC frameBufferRTVDesc = {};
	frameBufferRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	frameBufferRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &frameBufferRTVDesc, &FrameBufferRTV);
}

void FD3DDevice::ReleaseFrameBuffer()
{
	SAFE_RELEASE(FrameBufferRTV);
	SAFE_RELEASE(FrameBuffer);
}

void FD3DDevice::CreateDepthStencilBuffer()
{
	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
	depthStencilDesc.Width = static_cast<uint32>(ViewportInfo.Width);
	depthStencilDesc.Height = static_cast<uint32>(ViewportInfo.Height);
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	//	Depth 24bit + Stencil 8bit
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Device->CreateTexture2D(&depthStencilDesc, nullptr, &DepthStencilBuffer);
	Device->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);
}

void FD3DDevice::ReleaseDepthStencilBuffer()
{
	SAFE_RELEASE(DepthStencilView);
	SAFE_RELEASE(DepthStencilBuffer);
}
