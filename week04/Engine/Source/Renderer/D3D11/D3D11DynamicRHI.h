#pragma once

#include "Renderer/D3D11/D3D11Common.h"
#include "Core/HAL/PlatformTypes.h"
#include <Windows.h>
#include <d3dcompiler.h>

class FD3D11DynamicRHI
{
  public:
    bool Initialize(HWND InWindowHandle);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    bool Resize(int32 InWidth, int32 InHeight);

    ID3D11Device*   GetDevice() const { return Device.Get(); }
    ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext.Get(); }
    IDXGISwapChain* GetSwapChain() const { return SwapChain.Get(); }

    ID3D11RenderTargetView* GetBackBufferRTV() const { return BackBufferRTV.Get(); }
    ID3D11Texture2D*        GetDepthStencilBuffer() const { return DepthStencilBuffer.Get(); }
    ID3D11DepthStencilView* GetDepthStencilView() const { return DepthStencilView.Get(); }

    D3D11_VIEWPORT GetViewport() const { return Viewport; }

    int32 GetViewportWidth() const { return ViewportWidth; }
    int32 GetViewportHeight() const { return ViewportHeight; }

    void SetViewport(int32 InWidth, int32 InHeight);
    void SetDefaultRenderTargets();
    void Clear(const FLOAT InClearColor[4], float InDepth = 1.0f, uint8 InStencil = 0);

    void SetVSyncEnabled(bool bInVSyncEnabled) { bVSyncEnabled = bInVSyncEnabled; }
    bool IsVSyncEnabled() const { return bVSyncEnabled; }

  public:
    bool CompileShaderFromFile(const wchar_t* InFilePath, const char* InEntryPoint,
                               const char* InTarget, ID3DBlob** OutShaderBlob) const;

    bool CreateVertexShaderAndInputLayout(const wchar_t* InFilePath, const char* InEntryPoint,
                                          const D3D11_INPUT_ELEMENT_DESC* InInputElements,
                                          uint32                          InInputElementCount,
                                          ID3D11VertexShader**            OutVertexShader,
                                          ID3D11InputLayout**             OutInputLayout) const;

    bool CreatePixelShader(const wchar_t* InFilePath, const char* InEntryPoint,
                           ID3D11PixelShader** OutPixelShader) const;

    bool CreateConstantBuffer(uint32 InByteWidth, ID3D11Buffer** OutConstantBuffer) const;
    bool UpdateConstantBuffer(ID3D11Buffer* InConstantBuffer, const void* InData,
                              uint32 InDataSize) const;
    bool UpdateDynamicBuffer(ID3D11Buffer* InBuffer, const void* InData, uint32 InDataSize) const;

    void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) const;
    void SetInputLayout(ID3D11InputLayout* InInputLayout) const;
    void SetVertexBuffer(uint32 InSlot, ID3D11Buffer* InVertexBuffer, uint32 InStride,
                         uint32 InOffset) const;
    void SetVertexBuffers(uint32 InStartSlot, uint32 InBufferCount, ID3D11Buffer* const* InBuffers,
                          const uint32* InStrides, const uint32* InOffsets) const;
    void SetIndexBuffer(ID3D11Buffer* InIndexBuffer, DXGI_FORMAT InFormat, uint32 InOffset) const;

    void SetVertexShader(ID3D11VertexShader* InVertexShader) const;
    void SetPixelShader(ID3D11PixelShader* InPixelShader) const;
    void SetVSConstantBuffer(uint32 InSlot, ID3D11Buffer* InConstantBuffer) const;
    void SetPSConstantBuffer(uint32 InSlot, ID3D11Buffer* InConstantBuffer) const;

    void SetRasterizerState(ID3D11RasterizerState* InRasterizerState) const;
    void SetDepthStencilState(ID3D11DepthStencilState* InDepthStencilState,
                              uint32 InStencilRef = 0) const;

    void Draw(uint32 InVertexCount, uint32 InStartVertexLocation = 0) const;
    void DrawIndexed(uint32 InIndexCount, uint32 InStartIndexLocation = 0,
                     int32 InBaseVertexLocation = 0) const;
    void DrawIndexedInstanced(uint32 InIndexCountPerInstance, uint32 InInstanceCount,
                              uint32 InStartIndexLocation = 0, int32 InBaseVertexLocation = 0,
                              uint32 InStartInstanceLocation = 0) const;

  private:
    bool CreateDeviceAndSwapChain(HWND InWindowHandle);
    bool CreateBackBuffer();
    bool CreateDepthStencilBuffer(int32 InWidth, int32 InHeight);
    void ReleaseBackBufferResources();

  private:
    HWND WindowHandle = nullptr;

    int32 ViewportWidth = 0;
    int32 ViewportHeight = 0;

    bool bVSyncEnabled = true;

    TComPtr<ID3D11Device>        Device;
    TComPtr<ID3D11DeviceContext> DeviceContext;
    TComPtr<IDXGISwapChain>      SwapChain;

    TComPtr<ID3D11Texture2D>        BackBufferTexture;
    TComPtr<ID3D11RenderTargetView> BackBufferRTV;

    TComPtr<ID3D11Texture2D>        DepthStencilBuffer;
    TComPtr<ID3D11DepthStencilView> DepthStencilView;

    D3D11_VIEWPORT Viewport = {};
};
