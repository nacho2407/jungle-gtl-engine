#pragma once

/*
	Direct3D Device, Context, Swapchain을 관리하는 Class 입니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Core/CoreTypes.h"

enum class EDepthStencilState
{
	Default,
	DepthReadOnly,
	StencilWrite,
	StencilWriteOnlyEqual,

	// --- 기즈모 전용 ---
	GizmoInside,         
	GizmoOutside         
};

enum class EBlendState
{
	Opaque,
	AlphaBlend,
	NoColor
};

enum class ERasterizerState
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
};

class FD3DDevice
{
private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11Texture2D* SelectionMaskBuffer = nullptr;
	ID3D11RenderTargetView* SelectionMaskRTV = nullptr;
	ID3D11ShaderResourceView* SelectionMaskSRV = nullptr;

	ID3D11RasterizerState* RasterizerStateBackCull = nullptr;
	ID3D11RasterizerState* RasterizerStateFrontCull = nullptr;
	ID3D11RasterizerState* RasterizerStateNoCull = nullptr;
	ID3D11RasterizerState* RasterizerStateWireFrame = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;

	ID3D11DepthStencilState* DepthStencilStateDefault = nullptr;
	ID3D11DepthStencilState* DepthStencilStateDepthReadOnly = nullptr;
	ID3D11DepthStencilState* DepthStencilStateStencilWrite = nullptr;
	ID3D11DepthStencilState* DepthStencilStateStencilMaskEqual = nullptr;

	ID3D11DepthStencilState* DepthStencilStateGizmoInside = nullptr;  
	ID3D11DepthStencilState* DepthStencilStateGizmoOutside = nullptr; 

	ID3D11BlendState* BlendStateAlpha = nullptr;
	ID3D11BlendState* BlendStateNoColorWrite = nullptr;

	D3D11_VIEWPORT ViewportInfo = {};

	const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

	ERasterizerState CurrentRasterizerState = ERasterizerState::SolidBackCull;
	EDepthStencilState CurrentDepthStencilState = EDepthStencilState::Default;
	EBlendState CurrentBlendState = EBlendState::Opaque;

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;

public:


private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateRasterizerState();
	void ReleaseRasterizerState();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

	void CreateBlendState();
	void ReleaseBlendState();

public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();

	void BeginFrame();
	void EndFrame();

	void OnResizeViewport(int width, int height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;
	ID3D11RenderTargetView* GetFrameBufferRTV() const { return FrameBufferRTV; }
	ID3D11RenderTargetView* GetSelectionMaskRTV() const { return SelectionMaskRTV; }
	ID3D11ShaderResourceView* GetSelectionMaskSRV() const { return SelectionMaskSRV; }
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView; }
	float GetViewportWidth() const { return ViewportInfo.Width; }
	float GetViewportHeight() const { return ViewportInfo.Height; }

	void SetDepthStencilState(EDepthStencilState InState);
	void SetBlendState(EBlendState InState);
	void SetRasterizerState(ERasterizerState InState);
};

