#include "Core/HAL/PlatformTypes.h"
#include "Renderer/D3D11/D3D11DynamicRHI.h"

bool FD3D11DynamicRHI::Initialize(HWND InWindowHandle)
{
    if (InWindowHandle == nullptr)
    {
        return false;
    }

    WindowHandle = InWindowHandle;

    RECT ClientRect = {};
    if (!GetClientRect(WindowHandle, &ClientRect))
    {
        return false;
    }

    ViewportWidth = static_cast<int32>(ClientRect.right - ClientRect.left);
    ViewportHeight = static_cast<int32>(ClientRect.bottom - ClientRect.top);

    if (ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        return false;
    }

    if (!CreateDeviceAndSwapChain(WindowHandle))
    {
        return false;
    }

    if (!CreateBackBuffer())
    {
        Shutdown();
        return false;
    }

    if (!CreateDepthStencilBuffer(ViewportWidth, ViewportHeight))
    {
        Shutdown();
        return false;
    }

    SetViewport(ViewportWidth, ViewportHeight);
    SetDefaultRenderTargets();

    return true;
}

void FD3D11DynamicRHI::Shutdown()
{
    if (DeviceContext)
    {
        DeviceContext->ClearState();
        DeviceContext->Flush();
    }

    ReleaseBackBufferResources();

    SwapChain.Reset();
    DeviceContext.Reset();
    Device.Reset();

    WindowHandle = nullptr;
    ViewportWidth = 0;
    ViewportHeight = 0;
    Viewport = {};
}

void FD3D11DynamicRHI::BeginFrame() { SetDefaultRenderTargets(); }

void FD3D11DynamicRHI::EndFrame()
{
    if (SwapChain)
    {
        const UINT SyncInterval = bVSyncEnabled ? 1 : 0;
        SwapChain->Present(SyncInterval, 0);
    }
}

bool FD3D11DynamicRHI::Resize(int32 InWidth, int32 InHeight)
{
    if (!SwapChain || !Device || !DeviceContext)
    {
        return false;
    }

    if (InWidth <= 0 || InHeight <= 0)
    {
        return false;
    }

    if (ViewportWidth == InWidth && ViewportHeight == InHeight)
    {
        return true;
    }

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    DeviceContext->Flush();

    ReleaseBackBufferResources();

    HRESULT Hr = SwapChain->ResizeBuffers(0, static_cast<UINT>(InWidth),
                                          static_cast<UINT>(InHeight), DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(Hr))
    {
        return false;
    }

    ViewportWidth = InWidth;
    ViewportHeight = InHeight;

    if (!CreateBackBuffer())
    {
        return false;
    }

    if (!CreateDepthStencilBuffer(ViewportWidth, ViewportHeight))
    {
        return false;
    }

    SetViewport(ViewportWidth, ViewportHeight);
    SetDefaultRenderTargets();

    return true;
}

void FD3D11DynamicRHI::SetViewport(int32 InWidth, int32 InHeight)
{
    ViewportWidth = InWidth;
    ViewportHeight = InHeight;

    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width = static_cast<FLOAT>(InWidth);
    Viewport.Height = static_cast<FLOAT>(InHeight);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    if (DeviceContext)
    {
        DeviceContext->RSSetViewports(1, &Viewport);
    }
}

void FD3D11DynamicRHI::SetDefaultRenderTargets()
{
    if (!DeviceContext || !BackBufferRTV || !DepthStencilView)
    {
        return;
    }

    ID3D11RenderTargetView* RTV = BackBufferRTV.Get();
    DeviceContext->OMSetRenderTargets(1, &RTV, DepthStencilView.Get());
    DeviceContext->RSSetViewports(1, &Viewport);
}

bool FD3D11DynamicRHI::CompileShaderFromFile(const wchar_t* InFilePath, const char* InEntryPoint,
                                             const char* InTarget, ID3DBlob** OutShaderBlob) const
{
    if (OutShaderBlob == nullptr)
    {
        return false;
    }

    *OutShaderBlob = nullptr;

    UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined(_DEBUG)
    CompileFlags |= D3DCOMPILE_DEBUG;
    CompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    TComPtr<ID3DBlob> ErrorBlob;

    HRESULT Hr =
        D3DCompileFromFile(InFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, InEntryPoint,
                           InTarget, CompileFlags, 0, OutShaderBlob, ErrorBlob.GetAddressOf());

    if (FAILED(Hr))
    {
        if (ErrorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
        }
        return false;
    }

    return true;
}

bool FD3D11DynamicRHI::CreateVertexShaderAndInputLayout(
    const wchar_t* InFilePath, const char* InEntryPoint,
    const D3D11_INPUT_ELEMENT_DESC* InInputElements, uint32 InInputElementCount,
    ID3D11VertexShader** OutVertexShader, ID3D11InputLayout** OutInputLayout) const
{
    if (OutVertexShader == nullptr || OutInputLayout == nullptr)
    {
        return false;
    }

    *OutVertexShader = nullptr;
    *OutInputLayout = nullptr;

    TComPtr<ID3DBlob> VSBlob;
    if (!CompileShaderFromFile(InFilePath, InEntryPoint, "vs_5_0", VSBlob.GetAddressOf()))
    {
        return false;
    }

    HRESULT Hr = Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(),
                                            nullptr, OutVertexShader);
    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateInputLayout(InInputElements, InInputElementCount, VSBlob->GetBufferPointer(),
                                   VSBlob->GetBufferSize(), OutInputLayout);
    if (FAILED(Hr))
    {
        if (*OutVertexShader)
        {
            (*OutVertexShader)->Release();
            *OutVertexShader = nullptr;
        }
        return false;
    }

    return true;
}

bool FD3D11DynamicRHI::CreatePixelShader(const wchar_t* InFilePath, const char* InEntryPoint,
                                         ID3D11PixelShader** OutPixelShader) const
{
    if (OutPixelShader == nullptr)
    {
        return false;
    }

    *OutPixelShader = nullptr;

    TComPtr<ID3DBlob> PSBlob;
    if (!CompileShaderFromFile(InFilePath, InEntryPoint, "ps_5_0", PSBlob.GetAddressOf()))
    {
        return false;
    }

    HRESULT Hr = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(),
                                           nullptr, OutPixelShader);

    return SUCCEEDED(Hr);
}

bool FD3D11DynamicRHI::CreateConstantBuffer(uint32         InByteWidth,
                                            ID3D11Buffer** OutConstantBuffer) const
{
    if (OutConstantBuffer == nullptr)
    {
        return false;
    }

    *OutConstantBuffer = nullptr;

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = (InByteWidth + 15u) & ~15u;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    HRESULT Hr = Device->CreateBuffer(&Desc, nullptr, OutConstantBuffer);
    return SUCCEEDED(Hr);
}

void FD3D11DynamicRHI::Clear(const FLOAT InClearColor[4], float InDepth, uint8 InStencil)
{
    if (DeviceContext && BackBufferRTV)
    {
        DeviceContext->ClearRenderTargetView(BackBufferRTV.Get(), InClearColor);
    }

    if (DeviceContext && DepthStencilView)
    {
        DeviceContext->ClearDepthStencilView(DepthStencilView.Get(),
                                             D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, InDepth,
                                             static_cast<UINT8>(InStencil));
    }
}

bool FD3D11DynamicRHI::CreateDeviceAndSwapChain(HWND InWindowHandle)
{
    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferDesc.Width = static_cast<UINT>(ViewportWidth);
    SwapChainDesc.BufferDesc.Height = static_cast<UINT>(ViewportHeight);
    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    SwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.OutputWindow = InWindowHandle;
    SwapChainDesc.Windowed = TRUE;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.Flags = 0;

    UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
    CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL CreatedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT Hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags, FeatureLevels,
        static_cast<UINT>(ARRAYSIZE(FeatureLevels)), D3D11_SDK_VERSION,
        &SwapChainDesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), &CreatedFeatureLevel,
        DeviceContext.GetAddressOf());

#ifdef _DEBUG
    if (FAILED(Hr))
    {
        CreateDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;

        Hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags, FeatureLevels,
            static_cast<UINT>(ARRAYSIZE(FeatureLevels)), D3D11_SDK_VERSION,
            &SwapChainDesc, SwapChain.GetAddressOf(), Device.GetAddressOf(), &CreatedFeatureLevel,
            DeviceContext.GetAddressOf());
    }
#endif

    return SUCCEEDED(Hr);
}

bool FD3D11DynamicRHI::CreateBackBuffer()
{
    if (!SwapChain || !Device)
    {
        return false;
    }

    HRESULT Hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(BackBufferTexture.GetAddressOf()));

    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateRenderTargetView(BackBufferTexture.Get(), nullptr,
                                        BackBufferRTV.GetAddressOf());

    return SUCCEEDED(Hr);
}

bool FD3D11DynamicRHI::CreateDepthStencilBuffer(int32 InWidth, int32 InHeight)
{
    if (!Device)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = static_cast<UINT>(InWidth);
    DepthDesc.Height = static_cast<UINT>(InHeight);
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.SampleDesc.Quality = 0;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    DepthDesc.CPUAccessFlags = 0;
    DepthDesc.MiscFlags = 0;

    HRESULT Hr = Device->CreateTexture2D(&DepthDesc, nullptr, DepthStencilBuffer.GetAddressOf());

    if (FAILED(Hr))
    {
        return false;
    }

    Hr = Device->CreateDepthStencilView(DepthStencilBuffer.Get(), nullptr,
                                        DepthStencilView.GetAddressOf());

    return SUCCEEDED(Hr);
}

void FD3D11DynamicRHI::ReleaseBackBufferResources()
{
    DepthStencilView.Reset();
    DepthStencilBuffer.Reset();

    BackBufferRTV.Reset();
    BackBufferTexture.Reset();
}

bool FD3D11DynamicRHI::UpdateConstantBuffer(ID3D11Buffer* InConstantBuffer, const void* InData,
                                            uint32 InDataSize) const
{
    return UpdateDynamicBuffer(InConstantBuffer, InData, InDataSize);
}

bool FD3D11DynamicRHI::UpdateDynamicBuffer(ID3D11Buffer* InBuffer, const void* InData,
                                           uint32 InDataSize) const
{
    if (DeviceContext == nullptr || InBuffer == nullptr || InData == nullptr || InDataSize == 0)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    HRESULT Hr = DeviceContext->Map(InBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (FAILED(Hr))
    {
        return false;
    }

    memcpy(MappedResource.pData, InData, InDataSize);
    DeviceContext->Unmap(InBuffer, 0);

    return true;
}

void FD3D11DynamicRHI::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology) const
{
    if (DeviceContext)
    {
        DeviceContext->IASetPrimitiveTopology(InTopology);
    }
}

void FD3D11DynamicRHI::SetInputLayout(ID3D11InputLayout* InInputLayout) const
{
    if (DeviceContext)
    {
        DeviceContext->IASetInputLayout(InInputLayout);
    }
}

void FD3D11DynamicRHI::SetVertexBuffer(uint32 InSlot, ID3D11Buffer* InVertexBuffer, uint32 InStride,
                                       uint32 InOffset) const
{
    if (DeviceContext == nullptr)
    {
        return;
    }

    UINT Slot = static_cast<UINT>(InSlot);
    UINT Stride = static_cast<UINT>(InStride);
    UINT Offset = static_cast<UINT>(InOffset);
    DeviceContext->IASetVertexBuffers(Slot, 1, &InVertexBuffer, &Stride, &Offset);
}

void FD3D11DynamicRHI::SetVertexBuffers(uint32 InStartSlot, uint32 InBufferCount,
                                        ID3D11Buffer* const* InBuffers, const uint32* InStrides,
                                        const uint32* InOffsets) const
{
    if (DeviceContext == nullptr || InBuffers == nullptr || InStrides == nullptr ||
        InOffsets == nullptr || InBufferCount == 0)
    {
        return;
    }

    UINT Strides[8] = {};
    UINT Offsets[8] = {};
    if (InBufferCount > 8)
    {
        return;
    }

    for (uint32 Index = 0; Index < InBufferCount; ++Index)
    {
        Strides[Index] = static_cast<UINT>(InStrides[Index]);
        Offsets[Index] = static_cast<UINT>(InOffsets[Index]);
    }

    DeviceContext->IASetVertexBuffers(static_cast<UINT>(InStartSlot), static_cast<UINT>(InBufferCount),
                                      InBuffers, Strides, Offsets);
}

void FD3D11DynamicRHI::SetIndexBuffer(ID3D11Buffer* InIndexBuffer, DXGI_FORMAT InFormat,
                                      uint32 InOffset) const
{
    if (DeviceContext)
    {
        DeviceContext->IASetIndexBuffer(InIndexBuffer, InFormat, static_cast<UINT>(InOffset));
    }
}

void FD3D11DynamicRHI::SetVertexShader(ID3D11VertexShader* InVertexShader) const
{
    if (DeviceContext)
    {
        DeviceContext->VSSetShader(InVertexShader, nullptr, 0);
    }
}

void FD3D11DynamicRHI::SetPixelShader(ID3D11PixelShader* InPixelShader) const
{
    if (DeviceContext)
    {
        DeviceContext->PSSetShader(InPixelShader, nullptr, 0);
    }
}

void FD3D11DynamicRHI::SetVSConstantBuffer(uint32 InSlot, ID3D11Buffer* InConstantBuffer) const
{
    if (DeviceContext == nullptr)
    {
        return;
    }

    UINT Slot = static_cast<UINT>(InSlot);
    DeviceContext->VSSetConstantBuffers(Slot, 1, &InConstantBuffer);
}

void FD3D11DynamicRHI::SetPSConstantBuffer(uint32 InSlot, ID3D11Buffer* InConstantBuffer) const
{
    if (DeviceContext == nullptr)
    {
        return;
    }

    UINT Slot = static_cast<UINT>(InSlot);
    DeviceContext->PSSetConstantBuffers(Slot, 1, &InConstantBuffer);
}

void FD3D11DynamicRHI::SetRasterizerState(ID3D11RasterizerState* InRasterizerState) const
{
    if (DeviceContext)
    {
        DeviceContext->RSSetState(InRasterizerState);
    }
}

void FD3D11DynamicRHI::SetDepthStencilState(ID3D11DepthStencilState* InDepthStencilState,
                                            uint32 InStencilRef) const
{
    if (DeviceContext)
    {
        DeviceContext->OMSetDepthStencilState(InDepthStencilState, static_cast<UINT>(InStencilRef));
    }
}

void FD3D11DynamicRHI::Draw(uint32 InVertexCount, uint32 InStartVertexLocation) const
{
    if (DeviceContext)
    {
        DeviceContext->Draw(static_cast<UINT>(InVertexCount), static_cast<UINT>(InStartVertexLocation));
    }
}

void FD3D11DynamicRHI::DrawIndexed(uint32 InIndexCount, uint32 InStartIndexLocation,
                                   int32 InBaseVertexLocation) const
{
    if (DeviceContext)
    {
        DeviceContext->DrawIndexed(static_cast<UINT>(InIndexCount), static_cast<UINT>(InStartIndexLocation),
                                   static_cast<INT>(InBaseVertexLocation));
    }
}

void FD3D11DynamicRHI::DrawIndexedInstanced(uint32 InIndexCountPerInstance, uint32 InInstanceCount,
                                            uint32 InStartIndexLocation, int32 InBaseVertexLocation,
                                            uint32 InStartInstanceLocation) const
{
    if (DeviceContext)
    {
        DeviceContext->DrawIndexedInstanced(static_cast<UINT>(InIndexCountPerInstance),
                                            static_cast<UINT>(InInstanceCount),
                                            static_cast<UINT>(InStartIndexLocation),
                                            static_cast<INT>(InBaseVertexLocation),
                                            static_cast<UINT>(InStartInstanceLocation));
    }
}
