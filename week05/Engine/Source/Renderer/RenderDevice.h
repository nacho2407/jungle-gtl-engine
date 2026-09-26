#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

class ENGINE_API FRenderDevice
{
public:
	FRenderDevice() = default;
	~FRenderDevice()
	{
		Release();
	}

	// D3D11 디바이스, 스왑체인, 백버퍼 기반 뷰를 생성한다.
	bool Initialize(HWND InHwnd, int32 Width, int32 Height)
	{
		Hwnd = InHwnd;
		if (!CreateDeviceAndSwapChain(InHwnd, Width, Height))
		{
			return false;
		}

		if (!CreateRenderTargetAndDepthStencil(Width, Height))
		{
			return false;
		}

		Viewport.TopLeftX = 0.0f;
		Viewport.TopLeftY = 0.0f;
		Viewport.Width = static_cast<float>(Width);
		Viewport.Height = static_cast<float>(Height);
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
		return true;
	}

	// 백버퍼와 깊이 버퍼를 비우고 스왑체인 타깃을 바인딩한다.
	void BeginFrame(const float ClearColor[4])
	{
		if (RenderTargetView && DeviceContext)
		{
			DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
		}
		if (DepthStencilView && DeviceContext)
		{
			DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}
		BindSwapChainRTV();
	}

	// 스왑체인을 Present하고 가려짐 상태를 갱신한다.
	void EndFrame()
	{
		BindSwapChainRTV();

		if (!SwapChain)
		{
			return;
		}

		const UINT SyncInterval = bVSyncEnabled ? 1u : 0u;
		const HRESULT Hr = SwapChain->Present(SyncInterval, 0);
		if (Hr == DXGI_STATUS_OCCLUDED)
		{
			bSwapChainOccluded = true;
		}
	}

	// 스왑체인 RTV/DSV와 기본 뷰포트를 출력 머저에 바인딩한다.
	void BindSwapChainRTV()
	{
		if (RenderTargetView && DeviceContext)
		{
			DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
			DeviceContext->RSSetViewports(1, &Viewport);
		}
	}

	// 현재 스왑체인이 가려진 상태인지 검사한다.
	bool IsOccluded()
	{
		if (bSwapChainOccluded && SwapChain && SwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			return true;
		}

		bSwapChainOccluded = false;
		return false;
	}

	// 리사이즈 후 스왑체인 크기에 맞는 렌더 타깃을 다시 만든다.
	void OnResize(int32 Width, int32 Height)
	{
		if (Width <= 0 || Height <= 0 || !SwapChain || !DeviceContext)
		{
			return;
		}

		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		if (RenderTargetView)
		{
			RenderTargetView->Release();
			RenderTargetView = nullptr;
		}

		if (DepthStencilView)
		{
			DepthStencilView->Release();
			DepthStencilView = nullptr;
		}

		SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
		if (CreateRenderTargetAndDepthStencil(Width, Height))
		{
			Viewport.Width = static_cast<float>(Width);
			Viewport.Height = static_cast<float>(Height);
		}
	}

	// 디바이스가 소유한 스왑체인과 백버퍼 자원을 모두 해제한다.
	void Release()
	{
		if (DepthStencilView)
		{
			DepthStencilView->Release();
			DepthStencilView = nullptr;
		}
		if (RenderTargetView)
		{
			RenderTargetView->Release();
			RenderTargetView = nullptr;
		}
		if (SwapChain)
		{
			SwapChain->Release();
			SwapChain = nullptr;
		}
		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}
		if (Device)
		{
			Device->Release();
			Device = nullptr;
		}
	}

	// Present 시 VSync 사용 여부를 설정한다.
	void SetVSync(bool bEnable) { bVSyncEnabled = bEnable; }
	// 현재 VSync 설정 상태를 반환한다.
	bool IsVSyncEnabled() const { return bVSyncEnabled; }

	// D3D11 디바이스 접근자다.
	ID3D11Device* GetDevice() const { return Device; }
	// D3D11 디바이스 컨텍스트 접근자다.
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	// 스왑체인 접근자다.
	IDXGISwapChain* GetSwapChain() const { return SwapChain; }
	// 백버퍼 RTV 접근자다.
	ID3D11RenderTargetView* GetRenderTargetView() const { return RenderTargetView; }
	// 백버퍼 DSV 접근자다.
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView; }
	// 렌더 대상 윈도우 핸들을 반환한다.
	HWND GetHwnd() const { return Hwnd; }
	// 백버퍼 전체를 덮는 기본 뷰포트를 반환한다.
	const D3D11_VIEWPORT& GetViewport() const { return Viewport; }

private:
	// D3D11 디바이스, 컨텍스트, 스왑체인을 생성한다.
	bool CreateDeviceAndSwapChain(HWND InHwnd, int32 Width, int32 Height)
	{
		DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
		SwapChainDesc.BufferDesc.Width = static_cast<UINT>(Width);
		SwapChainDesc.BufferDesc.Height = static_cast<UINT>(Height);
		SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.BufferCount = 2;
		SwapChainDesc.OutputWindow = InHwnd;
		SwapChainDesc.Windowed = TRUE;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
		CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;
		const HRESULT Hr = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			CreateDeviceFlags,
			&FeatureLevel,
			1,
			D3D11_SDK_VERSION,
			&SwapChainDesc,
			&SwapChain,
			&Device,
			nullptr,
			&DeviceContext);

		if (FAILED(Hr))
		{
			MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChain Failed.", nullptr, 0);
			return false;
		}

		return true;
	}

	// 백버퍼 RTV와 대응하는 깊이 스텐실 뷰를 생성한다.
	bool CreateRenderTargetAndDepthStencil(int32 Width, int32 Height)
	{
		if (!Device || !SwapChain)
		{
			return false;
		}

		ID3D11Texture2D* BackBuffer = nullptr;
		HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer));
		if (FAILED(Hr) || !BackBuffer)
		{
			return false;
		}

		Hr = Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);
		BackBuffer->Release();
		if (FAILED(Hr))
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC DepthDesc = {};
		DepthDesc.Width = static_cast<UINT>(Width);
		DepthDesc.Height = static_cast<UINT>(Height);
		DepthDesc.MipLevels = 1;
		DepthDesc.ArraySize = 1;
		DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthDesc.SampleDesc.Count = 1;
		DepthDesc.Usage = D3D11_USAGE_DEFAULT;
		DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		ID3D11Texture2D* DepthTexture = nullptr;
		Hr = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthTexture);
		if (FAILED(Hr) || !DepthTexture)
		{
			return false;
		}

		Hr = Device->CreateDepthStencilView(DepthTexture, nullptr, &DepthStencilView);
		DepthTexture->Release();
		return SUCCEEDED(Hr);
	}

private:
	HWND Hwnd = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11RenderTargetView* RenderTargetView = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	D3D11_VIEWPORT Viewport = {};
	bool bSwapChainOccluded = false;
	bool bVSyncEnabled = false;
};

