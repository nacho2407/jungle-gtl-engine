#pragma once

#include "D3D11/D3D11Common.h"
#include "HAL/PlatformTypes.h"
#include <Windows.h>

class FD3D11DynamicRHI
{
  public:
    bool Initialize(HWND InWindowHandle);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    bool Resize(int32 InWidth, int32 InHeight);

    ID3D11Device        *GetDevice() const { return Device.Get(); }
    ID3D11DeviceContext *GetDeviceContext() const { return DeviceContext.Get(); }
    IDXGISwapChain      *GetSwapChain() const { return SwapChain.Get(); }

    ID3D11RenderTargetView *GetBackBufferRTV() const { return BackBufferRTV.Get(); }
    ID3D11Texture2D        *GetDepthStencilBuffer() const { return DepthStencilBuffer.Get(); }
    ID3D11DepthStencilView *GetDepthStencilView() const { return DepthStencilView.Get(); }

    D3D11_VIEWPORT GetViewport() const { return Viewport; }

    int32 GetViewportWidth() const { return ViewportWidth; }
    int32 GetViewportHeight() const { return ViewportHeight; }

    void SetViewport(int32 InWidth, int32 InHeight);
    void SetDefaultRenderTargets();
    void Clear(const FLOAT InClearColor[4], float InDepth = 1.0f, uint8 InStencil = 0);

  private:
    bool CreateDeviceAndSwapChain(HWND InWindowHandle);
    bool CreateBackBuffer();
    bool CreateDepthStencilBuffer(int32 InWidth, int32 InHeight);
    void ReleaseBackBufferResources();

  private:
    HWND WindowHandle = nullptr;

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;

    TComPtr<ID3D11Device>        Device;
    TComPtr<ID3D11DeviceContext> DeviceContext;
    TComPtr<IDXGISwapChain>      SwapChain;

    TComPtr<ID3D11Texture2D>        BackBufferTexture;
    TComPtr<ID3D11RenderTargetView> BackBufferRTV;

    TComPtr<ID3D11Texture2D>        DepthStencilBuffer;
    TComPtr<ID3D11DepthStencilView> DepthStencilView;

    D3D11_VIEWPORT Viewport = {};
};