#pragma once
#include "Render/Types/RenderTypes.h"
#include <d3d11_1.h>
#include "Render/Types/RenderStateTypes.h"
#include "Core/CoreTypes.h"

#include "Render/D3D11/State/RasterizerStateManager.h"
#include "Render/D3D11/State/BlendStateManager.h"
#include "Render/D3D11/State/DepthStencilStateManager.h"

class FD3DDevice
{
public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();

	void Present();
	void OnResizeViewport(int width, int height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;
	ID3DUserDefinedAnnotation* GetUserDefinedAnnotation() const { return UserDefinedAnnotation; }
	ID3D11RenderTargetView* GetFrameBufferRTV() const { return FrameBufferRTV; }
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView; }
	const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }
	const float* GetClearColor() const { return ClearColor; }

	void SetDepthStencilState(EDepthStencilState InState);
	void SetBlendState(EBlendState InState);
	void SetRasterizerState(ERasterizerState InState);

private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3DUserDefinedAnnotation* UserDefinedAnnotation = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	FRasterizerStateManager RasterizerStateManager;
	FDepthStencilStateManager DepthStencilStateManager;
	FBlendStateManager BlendStateManager;

	D3D11_VIEWPORT ViewportInfo = {};

	const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;
};
