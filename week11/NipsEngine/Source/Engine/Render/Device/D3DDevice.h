#pragma once

/*
	Direct3D Device, Context, Swapchain을 관리하는 Class 입니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Core/CoreTypes.h"

struct ID3D11Debug;

/**
 * 데이터 전달용 구조체 (소유 데이터 아님)
 */
struct FRenderTargetSet
{
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
    ID3D11ShaderResourceView* SceneColorSRV = nullptr;
    ID3D11RenderTargetView*   SceneNormalRTV = nullptr;
    ID3D11ShaderResourceView*     SceneNormalSRV = nullptr;
    ID3D11RenderTargetView*     SceneLightRTV = nullptr;
    ID3D11ShaderResourceView*     SceneLightSRV = nullptr;
    ID3D11RenderTargetView*       SceneFogRTV = nullptr;
    ID3D11ShaderResourceView*     SceneFogSRV = nullptr;
    ID3D11RenderTargetView*       SceneWorldPosRTV = nullptr;
    ID3D11ShaderResourceView*     SceneWorldPosSRV = nullptr;
    ID3D11RenderTargetView*       SceneFXAARTV = nullptr;
    ID3D11ShaderResourceView*     SceneFXAASRV = nullptr;
	ID3D11RenderTargetView* SelectionMaskRTV = nullptr;
	ID3D11ShaderResourceView* SelectionMaskSRV = nullptr;
    ID3D11DepthStencilView*   DepthStencilView = nullptr;
    ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
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

	TComPtr<ID3D11Texture2D> DepthStencilBuffer;
	TComPtr<ID3D11DepthStencilView> DepthStencilView;

	TComPtr<ID3D11Debug> DebugDevice;

	D3D11_VIEWPORT ViewportInfo = {};

	const float ClearColor[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
	const float ClearNormal[4] = { 0.25f, 0.25f, 0.25f, 0.f };

	ID3D11RasterizerState* CurrentRasterizerState = nullptr;
	ID3D11DepthStencilState* CurrentDepthStencilState = nullptr;
	ID3D11BlendState* CurrentBlendState = nullptr;

	BOOL bTearingSupported = FALSE;
	UINT SwapChainFlags = 0;

private:
	void CreateDeviceAndSwapChain(HWND InHWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateDepthStencilBuffer();
	void ReleaseDepthStencilBuffer();

public:
	FD3DDevice() = default;

	void Create(HWND InHWindow);
	void Release();
	void ReportLiveObjects();

	void BeginFrame();
	void EndFrame();

	// 단일 Viewport 개선 중 임시 함수
	// 입력 RenderTarget 에 대한 BeginFrame 설정 수행
	void BeginViewportFrame(FRenderTargetSet& InRenderTargetSet);

	void OnResizeViewport(int width, int height);

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
	float GetViewportWidth() const { return ViewportInfo.Width; }
	float GetViewportHeight() const { return ViewportInfo.Height; }
	FRenderTargetSet GetBackBufferRenderTargets() const;
};

