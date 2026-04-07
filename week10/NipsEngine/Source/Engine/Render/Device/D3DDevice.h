#pragma once

/*
	Direct3D Device, Context, Swapchain을 관리하는 Class 입니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Core/CoreTypes.h"

struct ID3D11Debug;

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

struct FRenderTargetSet
{
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11ShaderResourceView* SceneColorSRV = nullptr;
	ID3D11RenderTargetView* SelectionMaskRTV = nullptr;
	ID3D11ShaderResourceView* SelectionMaskSRV = nullptr;
	ID3D11DepthStencilView* DepthStencilView = nullptr;
	float Width = 0.0f;
	float Height = 0.0f;

	bool IsValid() const
	{
		return SceneColorRTV != nullptr && DepthStencilView != nullptr && Width > 0.0f && Height > 0.0f;
	}
};

class FD3DDevice
{
private:
	TComPtr<ID3D11Device> Device;
	TComPtr<ID3D11DeviceContext> DeviceContext;
	TComPtr<IDXGISwapChain> SwapChain;

	TComPtr<ID3D11Texture2D> FrameBuffer;
	TComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	TComPtr<ID3D11Texture2D> SelectionMaskBuffer;
	TComPtr<ID3D11RenderTargetView> SelectionMaskRTV;
	TComPtr<ID3D11ShaderResourceView> SelectionMaskSRV;
	TComPtr<ID3D11Texture2D> ViewportSceneColorTexture;
	TComPtr<ID3D11RenderTargetView> ViewportSceneColorRTV;
	TComPtr<ID3D11ShaderResourceView> ViewportSceneColorSRV;
	TComPtr<ID3D11Texture2D> ViewportSelectionMaskTexture;
	TComPtr<ID3D11RenderTargetView> ViewportSelectionMaskRTV;
	TComPtr<ID3D11ShaderResourceView> ViewportSelectionMaskSRV;

	TComPtr<ID3D11RasterizerState> RasterizerStateBackCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateFrontCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateNoCull;
	TComPtr<ID3D11RasterizerState> RasterizerStateWireFrame;

	TComPtr<ID3D11Texture2D> DepthStencilBuffer;
	TComPtr<ID3D11DepthStencilView> DepthStencilView;
	TComPtr<ID3D11Texture2D> ViewportDepthStencilTexture;
	TComPtr<ID3D11DepthStencilView> ViewportDepthStencilView;

	TComPtr<ID3D11DepthStencilState> DepthStencilStateDefault;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateDepthReadOnly;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateStencilWrite;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateStencilMaskEqual;

	TComPtr<ID3D11DepthStencilState> DepthStencilStateGizmoInside;
	TComPtr<ID3D11DepthStencilState> DepthStencilStateGizmoOutside;

	TComPtr<ID3D11BlendState> BlendStateAlpha;
	TComPtr<ID3D11BlendState> BlendStateNoColorWrite;

	TComPtr<ID3D11Debug> DebugDevice;

	D3D11_VIEWPORT ViewportInfo = {};

	const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };

	ERasterizerState CurrentRasterizerState = ERasterizerState::SolidBackCull;
	EDepthStencilState CurrentDepthStencilState = EDepthStencilState::Default;
	EBlendState CurrentBlendState = EBlendState::Opaque;

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;
	uint32 ViewportRenderTargetWidth = 0;
	uint32 ViewportRenderTargetHeight = 0;

public:


private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateViewportRenderTargets(uint32 Width, uint32 Height);
	void ReleaseViewportRenderTargets();

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
	void ReportLiveObjects();

	void BeginFrame();
	void EndFrame();

	void OnResizeViewport(int width, int height);
	void EnsureViewportRenderTargets(int width, int height);

	/*
	 * 렌더링 대상 : 지정한 서브 영역으로 제한
	 * 다중 뷰포트 렌더링 시 각 뷰포트마다 호출.
	 * BeginFrame 이후, 각 뷰포트 렌더 직전에 호출해야 합니다.
	 */
	void SetSubViewport(int32 X, int32 Y, int32 Width, int32 Height);

	ID3D11Device* GetDevice() const;
	ID3D11DeviceContext* GetDeviceContext() const;
	ID3D11RenderTargetView* GetFrameBufferRTV() const { return FrameBufferRTV.Get(); }
	ID3D11RenderTargetView* GetSelectionMaskRTV() const { return SelectionMaskRTV.Get(); }
	ID3D11ShaderResourceView* GetSelectionMaskSRV() const { return SelectionMaskSRV.Get(); }
	ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView.Get(); }
	ID3D11ShaderResourceView* GetViewportSceneColorSRV() const { return ViewportSceneColorSRV.Get(); }
	float GetViewportWidth() const { return ViewportInfo.Width; }
	float GetViewportHeight() const { return ViewportInfo.Height; }
	FRenderTargetSet GetBackBufferRenderTargets() const;
	FRenderTargetSet GetViewportRenderTargets() const;

	void SetDepthStencilState(EDepthStencilState InState);
	void SetBlendState(EBlendState InState);
	void SetRasterizerState(ERasterizerState InState);
};

