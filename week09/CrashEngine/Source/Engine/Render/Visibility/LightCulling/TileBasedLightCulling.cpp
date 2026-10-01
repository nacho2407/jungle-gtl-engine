// 렌더 영역의 세부 동작을 구현합니다.
#include "TileBasedLightCulling.h"
#include "Viewport/ViewportClient.h"
#include "Viewport/Viewport.h"
#include "Render/RHI/D3D11/Device/D3DDevice.h"
#include "Render/RHI/D3D11/Buffers/ConstantBuffer.h"
#include "Render/RHI/D3D11/Shaders/ShaderProgramBase.h"
#include "Render/Resources/Shaders/ShaderProgramDesc.h"

#include <d3dcompiler.h>

#define TILE_SIZE 4
#define MAX_LIGHTS_PER_TILE 1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32) // 32

// ============================================================
// Initialize / Release
// ============================================================

FTileBasedLightCulling::~FTileBasedLightCulling()
{
    Release();
}

void FTileBasedLightCulling::Initialize(FD3DDevice* InDevice)
{
    ENGINE_CHECK(InDevice);
    Device = InDevice;

    auto CompileCS = [&](D3D_SHADER_MACRO* macros, ID3D11ComputeShader** outCS)
    {
        ID3DBlob* CSBlob  = nullptr;

        FShaderStageDesc StageDesc = {};
        StageDesc.FilePath         = "Shaders/Passes/Visibility/LightCullingPassCS.hlsl";
        StageDesc.EntryPoint       = "CS_LightCulling";
        for (const D3D_SHADER_MACRO* Macro = macros; Macro && Macro->Name; ++Macro)
        {
            StageDesc.Defines.push_back({ Macro->Name, Macro->Definition ? Macro->Definition : "" });
        }

        if (!FShaderProgramBase::CompileShaderBlobStandalone(&CSBlob, StageDesc, "cs_5_0", "Compute Shader Compile Error"))
        {
            ENGINE_CHECK(false);
        }

        const HRESULT hr = Device->GetDevice()->CreateComputeShader(
            CSBlob->GetBufferPointer(),
            CSBlob->GetBufferSize(),
            nullptr,
            outCS);

        CSBlob->Release();
        ENGINE_CHECK(SUCCEEDED(hr));
    };

    D3D_SHADER_MACRO tileMacros[] = {
        { "TILE_BASED_CULLING", "1" },
        { "TILE_25D_CULLING_", "0" },
        { nullptr, nullptr }
    };

    CompileCS(tileMacros, &LightCullingCS_Tile);

    D3D_SHADER_MACRO tile25DMacros[] = {
        { "TILE_BASED_CULLING", "0" },
        { "TILE_25D_CULLING_", "1" },
        { nullptr, nullptr }
    };

    CompileCS(tile25DMacros, &LightCullingCS_25D);

    LightCullingParamsCBWrapper = new FConstantBuffer();
    LightCullingParamsCBWrapper->Create(Device->GetDevice(), sizeof(FLightCullingCBData));
    LightCullingParamsCB = LightCullingParamsCBWrapper->GetBuffer();
}

void FTileBasedLightCulling::Release()
{
    auto SafeRelease = [](auto*& ptr)
    {
        if (ptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    };

    // SafeRelease(LightCullingCS);
    SafeRelease(LightCullingCS_Tile);
    SafeRelease(LightCullingCS_25D);
    if (LightCullingParamsCBWrapper)
    {
        LightCullingParamsCBWrapper->Release();
        delete LightCullingParamsCBWrapper;
        LightCullingParamsCBWrapper = nullptr;
    }
    LightCullingParamsCB = nullptr;

    // SafeRelease(PointLightDataSRV);
    // SafeRelease(PointLightBuffer);

    SafeRelease(PerTilePointLightIndexMaskSRV);
    SafeRelease(PerTilePointLightIndexMaskUAV);
    SafeRelease(PerTilePointLightIndexMaskBuffer);

    SafeRelease(CulledPointLightIndexMaskUAV);
    SafeRelease(CulledPointLightIndexMaskBuffer);

    SafeRelease(DebugHitMapSRV);
    SafeRelease(DebugHitMapUAV);
    SafeRelease(DebugHitMapTexture);

    Device = nullptr;
}

// ============================================================
// Public API
// ============================================================

void FTileBasedLightCulling::OnResize(uint32 InWidth, uint32 InHeight)
{
    if (CurrentWidth == InWidth && CurrentHeight == InHeight)
        return;

    CurrentWidth  = InWidth;
    CurrentHeight = InHeight;

    ResizeTiles(InWidth, InHeight);
}

void FTileBasedLightCulling::SetPointLightData(const uint32 InLightsCount)
{
    // Lights = InLights;
    LightCount = InLightsCount;
    CreatePointLightBufferGPU();
}

void FTileBasedLightCulling::ClearDebugHitMap()
{
    if (!IsInitialized() || !DebugHitMapUAV)
        return;
    float ClearColor[4] = { 0, 0, 0, 0 };
    Device->GetDeviceContext()->ClearUnorderedAccessViewFloat(DebugHitMapUAV, ClearColor);
}

void FTileBasedLightCulling::Dispatch(const FFrameContext& frameContext, bool bEnable25DCulling)
{
    if (!IsInitialized())
        return;

    const uint32 ViewWidth  = static_cast<uint32>(frameContext.ViewportWidth);
    const uint32 ViewHeight = static_cast<uint32>(frameContext.ViewportHeight);
    if (ViewWidth == 0 || ViewHeight == 0)
        return;

    OnResize(ViewWidth, ViewHeight);
    UpdateLightCullingParamsCB(frameContext, bEnable25DCulling);

    ID3D11DeviceContext* Context = Device->GetDeviceContext();

    float ClearColor[4] = { 0, 0, 0, 0 };
    Context->ClearUnorderedAccessViewFloat(DebugHitMapUAV, ClearColor);

    // 🎯 [여기 추가!] 타일 마스크 버퍼를 매 프레임 무조건 0으로 비워줍니다!
    UINT clearMask[4] = { 0, 0, 0, 0 };
    Context->ClearUnorderedAccessViewUint(PerTilePointLightIndexMaskUAV, clearMask);

    ID3D11ShaderResourceView*  NullSRVs1[1] = { nullptr };
    ID3D11UnorderedAccessView* NullUAVs1[4] = { nullptr, nullptr, nullptr, nullptr };
    Context->CSSetShaderResources(0, 1, NullSRVs1);
    Context->CSSetUnorderedAccessViews(0, 4, NullUAVs1, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);

    if (frameContext.ShowFlags.b25DCulling)
        Context->CSSetShader(LightCullingCS_25D, nullptr, 0);
    else
        Context->CSSetShader(LightCullingCS_Tile, nullptr, 0);

    // ID3D11ShaderResourceView* SRVs[] = { PointLightDataSRV };
    // Context->CSSetShaderResources(0, 1, SRVs);

    ID3D11UnorderedAccessView* UAVs[] = {
        nullptr,
        PerTilePointLightIndexMaskUAV, // u1
        CulledPointLightIndexMaskUAV,  // u2
        DebugHitMapUAV,                   // u3
    };
    Context->CSSetUnorderedAccessViews(0, 4, UAVs, nullptr);

    Context->CSSetConstantBuffers(2, 1, &LightCullingParamsCB);


    // ---- Dispatch ----
    const UINT GroupSizeX = (ViewWidth + TILE_SIZE - 1) / TILE_SIZE;
    const UINT GroupSizeY = (ViewHeight + TILE_SIZE - 1) / TILE_SIZE;
    Context->Dispatch(GroupSizeX, GroupSizeY, 1);

    ID3D11ShaderResourceView*  NullSRVs[1] = { nullptr };
    ID3D11UnorderedAccessView* NullUAVs[4] = { nullptr, nullptr, nullptr, nullptr };
    Context->CSSetShaderResources(0, 1, NullSRVs);
    Context->CSSetUnorderedAccessViews(0, 4, NullUAVs, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);
}


// ============================================================
// Private helpers
// ============================================================

void FTileBasedLightCulling::ResizeTiles(uint32 InWidth, uint32 InHeight)
{
    NumTilesX         = (InWidth + TILE_SIZE - 1) / TILE_SIZE;
    NumTilesY         = (InHeight + TILE_SIZE - 1) / TILE_SIZE;
    NumTiles          = NumTilesX * NumTilesY;
    NumBucketsPerTile = SHADER_ENTITY_TILE_BUCKET_COUNT;

    CreateTileMaskBuffers();
    CreateDebugHitMap(InWidth, InHeight);
}

void FTileBasedLightCulling::CreatePointLightBufferGPU()
{
    // if (PointLightDataSRV) { PointLightDataSRV->Release(); PointLightDataSRV = nullptr; }
    // if (PointLightBuffer)  { PointLightBuffer->Release();  PointLightBuffer  = nullptr; }

    // if (Lights.empty())
    //     return;

    // D3D11_BUFFER_DESC Desc = {};
    // Desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    // Desc.ByteWidth           = sizeof(FLocalLightCBData) * Lights.size();
    // Desc.Usage               = D3D11_USAGE_DEFAULT;
    // Desc.StructureByteStride = sizeof(FLocalLightCBData);
    // Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    // D3D11_SUBRESOURCE_DATA InitData = {};
    // InitData.pSysMem = Lights.data();

    // HRESULT hr = Device->GetDevice()->CreateBuffer(&Desc, &InitData, &PointLightBuffer);
    // ENGINE_CHECK(SUCCEEDED(hr));

    // D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    // SrvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    // SrvDesc.Format              = DXGI_FORMAT_UNKNOWN;
    // SrvDesc.Buffer.FirstElement = 0;
    // SrvDesc.Buffer.NumElements  = Lights.size();

    // hr = Device->GetDevice()->CreateShaderResourceView(PointLightBuffer, &SrvDesc, &PointLightDataSRV);
    // ENGINE_CHECK(SUCCEEDED(hr));
}

void FTileBasedLightCulling::CreateTileMaskBuffers()
{
    auto SafeRelease = [](auto*& ptr)
    { if (ptr) { ptr->Release(); ptr = nullptr; } };
    SafeRelease(PerTilePointLightIndexMaskSRV);
    SafeRelease(PerTilePointLightIndexMaskUAV);
    SafeRelease(PerTilePointLightIndexMaskBuffer);
    SafeRelease(CulledPointLightIndexMaskUAV);
    SafeRelease(CulledPointLightIndexMaskBuffer);

    auto CreateMaskBuffer = [&](
                                uint32                      NumElements,
                                ID3D11Buffer**              OutBuffer,
                                ID3D11UnorderedAccessView** OutUAV,
                                ID3D11ShaderResourceView**  OutSRV)
    {
        D3D11_BUFFER_DESC Desc   = {};
        Desc.ByteWidth           = sizeof(uint32) * NumElements;
        Desc.Usage               = D3D11_USAGE_DEFAULT;
        Desc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        Desc.StructureByteStride = sizeof(uint32);
        Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

        HRESULT hr = Device->GetDevice()->CreateBuffer(&Desc, nullptr, OutBuffer);
        ENGINE_CHECK(SUCCEEDED(hr));

        D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
        UavDesc.ViewDimension                    = D3D11_UAV_DIMENSION_BUFFER;
        UavDesc.Format                           = DXGI_FORMAT_UNKNOWN;
        UavDesc.Buffer.FirstElement              = 0;
        UavDesc.Buffer.NumElements               = NumElements;

        hr = Device->GetDevice()->CreateUnorderedAccessView(*OutBuffer, &UavDesc, OutUAV);
        ENGINE_CHECK(SUCCEEDED(hr));

        if (OutSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
            SrvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_BUFFER;
            SrvDesc.Format                          = DXGI_FORMAT_UNKNOWN;
            SrvDesc.Buffer.FirstElement             = 0;
            SrvDesc.Buffer.NumElements              = NumElements;

            hr = Device->GetDevice()->CreateShaderResourceView(*OutBuffer, &SrvDesc, OutSRV);
            ENGINE_CHECK(SUCCEEDED(hr));
        }
    };

    CreateMaskBuffer(
        NumTiles * NumBucketsPerTile,
        &PerTilePointLightIndexMaskBuffer,
        &PerTilePointLightIndexMaskUAV,
        &PerTilePointLightIndexMaskSRV);

    CreateMaskBuffer(
        NumBucketsPerTile,
        &CulledPointLightIndexMaskBuffer,
        &CulledPointLightIndexMaskUAV,
        nullptr);
}

void FTileBasedLightCulling::CreateDebugHitMap(uint32 InWidth, uint32 InHeight)
{
    auto SafeRelease = [](auto*& ptr)
    { if (ptr) { ptr->Release(); ptr = nullptr; } };
    SafeRelease(DebugHitMapSRV);
    SafeRelease(DebugHitMapUAV);
    SafeRelease(DebugHitMapTexture);

    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width                = InWidth;
    TexDesc.Height               = InHeight;
    TexDesc.MipLevels            = 1;
    TexDesc.ArraySize            = 1;
    TexDesc.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT; // float4 (RWTexture2D<float4>)
    TexDesc.SampleDesc.Count     = 1;
    TexDesc.SampleDesc.Quality   = 0;
    TexDesc.Usage                = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags            = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->GetDevice()->CreateTexture2D(&TexDesc, nullptr, &DebugHitMapTexture);
    ENGINE_CHECK(SUCCEEDED(hr));

    D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
    UavDesc.ViewDimension                    = D3D11_UAV_DIMENSION_TEXTURE2D;
    UavDesc.Format                           = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = Device->GetDevice()->CreateUnorderedAccessView(DebugHitMapTexture, &UavDesc, &DebugHitMapUAV);
    ENGINE_CHECK(SUCCEEDED(hr));

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Format                          = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SrvDesc.Texture2D.MipLevels             = 1;

    hr = Device->GetDevice()->CreateShaderResourceView(DebugHitMapTexture, &SrvDesc, &DebugHitMapSRV);
    ENGINE_CHECK(SUCCEEDED(hr));
}

void FTileBasedLightCulling::UpdateLightCullingParamsCB(const FFrameContext& frameContext, bool bEnable25DCulling)
{
    if (!LightCullingParamsCB)
        return;

    FLightCullingCBData Params = {};
    Params.ScreenSizeX         = static_cast<uint32>(frameContext.ViewportWidth);
    Params.ScreenSizeY         = static_cast<uint32>(frameContext.ViewportHeight);
    Params.TileSizeX           = TILE_SIZE;
    Params.TileSizeY           = TILE_SIZE;
    Params.Enable25DCulling    = bEnable25DCulling ? 1u : 0u;
    Params.NearZ               = frameContext.NearClip;
    Params.FarZ                = frameContext.FarClip;
    Params.NumLights           = (float)LightCount;

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    HRESULT                  hr     = Device->GetDeviceContext()->Map(LightCullingParamsCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(Mapped.pData, &Params, sizeof(Params));
        Device->GetDeviceContext()->Unmap(LightCullingParamsCB, 0);
    }
}
