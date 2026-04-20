#include "TileBasedLightCulling.h"
#include "Viewport/ViewportClient.h"
#include "Viewport/Viewport.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Pipeline/FrameContext.h"
#include <d3dcompiler.h>

#define TILE_SIZE                       4
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)   // 32

// ============================================================
// Initialize / Release
// ============================================================

void FTileBasedLightCulling::Initialize(FD3DDevice* InDevice)
{
    check(InDevice);
    Device = InDevice;

    // ---- Compute Shader 컴파일 ----
    ID3DBlob* CSBlob  = nullptr;
    ID3DBlob* ErrBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(
        L"Shaders/TileBasedLightCullingCS.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CS_LightCulling", "cs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &CSBlob, &ErrBlob
    );

    if (FAILED(hr))
    {
        if (ErrBlob)
        {
            // 컴파일 오류 로그 출력 (프로젝트 로깅 매크로로 교체)
            OutputDebugStringA((const char*)ErrBlob->GetBufferPointer());
            ErrBlob->Release();
        }
        check(false); // 셰이더 컴파일 실패
    }

    hr = Device->GetDevice()->CreateComputeShader(
        CSBlob->GetBufferPointer(), CSBlob->GetBufferSize(),
        nullptr, &LightCullingCS
    );
    CSBlob->Release();
    check(SUCCEEDED(hr));

    // ---- LightCullingParams 상수 버퍼 (b2) ----
    D3D11_BUFFER_DESC CBDesc = {};
    CBDesc.ByteWidth      = sizeof(FLightCullingParams);
    CBDesc.Usage          = D3D11_USAGE_DYNAMIC;
    CBDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = Device->GetDevice()->CreateBuffer(&CBDesc, nullptr, &LightCullingParamsCB);
    check(SUCCEEDED(hr));
}

void FTileBasedLightCulling::Release()
{
    auto SafeRelease = [](auto*& ptr)
    {
        if (ptr) { ptr->Release(); ptr = nullptr; }
    };

    SafeRelease(LightCullingCS);
    SafeRelease(LightCullingParamsCB);

    SafeRelease(PointLightDataSRV);
    SafeRelease(PointLightBuffer);

    SafeRelease(PerTilePointLightIndexMaskSRV);
    SafeRelease(PerTilePointLightIndexMaskOutUAV);
    SafeRelease(PerTilePointLightIndexMaskBuffer);

    SafeRelease(CulledPointLightIndexMaskOUTUAV);
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

void FTileBasedLightCulling::SetPointLightData(const TArray<FLocalLightInfo>& InLights)
{
    Lights = InLights;
    CreatePointLightBufferGPU();
}

void FTileBasedLightCulling::Dispatch(const FFrameContext& frameContext, bool bEnable25DCulling)
{
    if (!IsInitialized())
        return;

    OnResize(frameContext.ViewportWidth, frameContext.ViewportHeight);
    UpdateLightCullingParamsCB(frameContext, bEnable25DCulling);

    ID3D11DeviceContext* Context = Device->GetDeviceContext();

	//이전 바인딩 해제
    float ClearColor[4] = { 0, 0, 0, 0 };
    Context->ClearUnorderedAccessViewFloat(DebugHitMapUAV, ClearColor);

    // ---- CS 바인딩 ----
    Context->CSSetShader(LightCullingCS, nullptr, 0);

    // ---- SRV (t0: PointLight 데이터, t1: SceneDepth) ----
	// 현재 외부에서 주입중
    //ID3D11ShaderResourceView* SRVs[] = { PointLightDataSRV };
    //Context->CSSetShaderResources(0, 1, SRVs);

    // ---- UAV (u0: 미사용, u1: PerTile, u2: Culled, u3: HitMap) ----
    ID3D11UnorderedAccessView* UAVs[] = {
        nullptr,                           // u0 미사용
        PerTilePointLightIndexMaskOutUAV,  // u1
        CulledPointLightIndexMaskOUTUAV,   // u2
        DebugHitMapUAV,                    // u3
    };
    Context->CSSetUnorderedAccessViews(0, 4, UAVs, nullptr);

    // ---- 상수 버퍼 ----
    Context->CSSetConstantBuffers(2, 1, &LightCullingParamsCB);

    // ---- Dispatch ----
    const UINT GroupSizeX = (frameContext.ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    const UINT GroupSizeY = (frameContext.ViewportHeight + TILE_SIZE - 1) / TILE_SIZE;
    Context->Dispatch(GroupSizeX, GroupSizeY, 1);

    // ---- 바인딩 해제 ----
    ID3D11ShaderResourceView*  NullSRVs[1]  = { nullptr };
    ID3D11UnorderedAccessView* NullUAVs[4]  = { nullptr, nullptr, nullptr, nullptr };
    Context->CSSetShaderResources(0, 1, NullSRVs);
    Context->CSSetUnorderedAccessViews(0, 4, NullUAVs, nullptr);
    Context->CSSetShader(nullptr, nullptr, 0);
}

// ============================================================
// Private helpers
// ============================================================

void FTileBasedLightCulling::ResizeTiles(uint32 InWidth, uint32 InHeight)
{
    NumTilesX        = (InWidth  + TILE_SIZE - 1) / TILE_SIZE;
    NumTilesY        = (InHeight + TILE_SIZE - 1) / TILE_SIZE;
    NumTiles         = NumTilesX * NumTilesY;
    NumBucketsPerTile = SHADER_ENTITY_TILE_BUCKET_COUNT;

    CreateTileMaskBuffers();
    CreateDebugHitMap(InWidth, InHeight);
}

void FTileBasedLightCulling::CreatePointLightBufferGPU()
{
    // 기존 리소스 해제
    if (PointLightDataSRV) { PointLightDataSRV->Release(); PointLightDataSRV = nullptr; }
    if (PointLightBuffer)  { PointLightBuffer->Release();  PointLightBuffer  = nullptr; }

    if (Lights.empty())
        return;

    // StructuredBuffer 생성
    D3D11_BUFFER_DESC Desc = {};
    Desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    Desc.ByteWidth           = sizeof(FLocalLightInfo) * Lights.size();
    Desc.Usage               = D3D11_USAGE_DEFAULT;
    Desc.StructureByteStride = sizeof(FLocalLightInfo);
    Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = Lights.data();

    HRESULT hr = Device->GetDevice()->CreateBuffer(&Desc, &InitData, &PointLightBuffer);
    check(SUCCEEDED(hr));

    // SRV (셰이더 t0)
    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    SrvDesc.Format              = DXGI_FORMAT_UNKNOWN;
    SrvDesc.Buffer.FirstElement = 0;
    SrvDesc.Buffer.NumElements  = Lights.size();

    hr = Device->GetDevice()->CreateShaderResourceView(PointLightBuffer, &SrvDesc, &PointLightDataSRV);
    check(SUCCEEDED(hr));
}

void FTileBasedLightCulling::CreateTileMaskBuffers()
{
    // 기존 리소스 해제
    auto SafeRelease = [](auto*& ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } };
    SafeRelease(PerTilePointLightIndexMaskSRV);
    SafeRelease(PerTilePointLightIndexMaskOutUAV);
    SafeRelease(PerTilePointLightIndexMaskBuffer);
    SafeRelease(CulledPointLightIndexMaskOUTUAV);
    SafeRelease(CulledPointLightIndexMaskBuffer);

    // 공통 Buffer + UAV + SRV 생성 람다
    auto CreateMaskBuffer = [&](
        uint32                      NumElements,
        ID3D11Buffer**              OutBuffer,
        ID3D11UnorderedAccessView** OutUAV,
        ID3D11ShaderResourceView**  OutSRV)
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth           = sizeof(uint32) * NumElements;
        Desc.Usage               = D3D11_USAGE_DEFAULT;
        Desc.BindFlags           = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        Desc.StructureByteStride = sizeof(uint32);
        Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

        HRESULT hr = Device->GetDevice()->CreateBuffer(&Desc, nullptr, OutBuffer);
        check(SUCCEEDED(hr));

        D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
        UavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
        UavDesc.Format              = DXGI_FORMAT_UNKNOWN;
        UavDesc.Buffer.FirstElement = 0;
        UavDesc.Buffer.NumElements  = NumElements;

        hr = Device->GetDevice()->CreateUnorderedAccessView(*OutBuffer, &UavDesc, OutUAV);
        check(SUCCEEDED(hr));

        if (OutSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
            SrvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
            SrvDesc.Format              = DXGI_FORMAT_UNKNOWN;
            SrvDesc.Buffer.FirstElement = 0;
            SrvDesc.Buffer.NumElements  = NumElements;

            hr = Device->GetDevice()->CreateShaderResourceView(*OutBuffer, &SrvDesc, OutSRV);
            check(SUCCEEDED(hr));
        }
    };

    // u1: 타일별 조명 마스크 (NumTiles * BucketCount 개의 uint)
    CreateMaskBuffer(
        NumTiles * NumBucketsPerTile,
        &PerTilePointLightIndexMaskBuffer,
        &PerTilePointLightIndexMaskOutUAV,
        &PerTilePointLightIndexMaskSRV     // PS에서 읽기 위해 SRV도 생성
    );

    // u2: 전체 OR 누적 마스크 (BucketCount 개의 uint, Shadow Map 최적화용)
    CreateMaskBuffer(
        NumBucketsPerTile,
        &CulledPointLightIndexMaskBuffer,
        &CulledPointLightIndexMaskOUTUAV,
        nullptr                            // CPU 스테이징으로 읽으므로 SRV 불필요
    );
}

void FTileBasedLightCulling::CreateDebugHitMap(uint32 InWidth, uint32 InHeight)
{
    auto SafeRelease = [](auto*& ptr) { if (ptr) { ptr->Release(); ptr = nullptr; } };
    SafeRelease(DebugHitMapSRV);
    SafeRelease(DebugHitMapUAV);
    SafeRelease(DebugHitMapTexture);

    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width              = InWidth;
    TexDesc.Height             = InHeight;
    TexDesc.MipLevels          = 1;
    TexDesc.ArraySize          = 1;
    TexDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT; // float4 (RWTexture2D<float4>)
    TexDesc.SampleDesc.Count   = 1;
    TexDesc.SampleDesc.Quality = 0;
    TexDesc.Usage              = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags          = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->GetDevice()->CreateTexture2D(&TexDesc, nullptr, &DebugHitMapTexture);
    check(SUCCEEDED(hr));

    // UAV (셰이더 u3)
    D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
    UavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    UavDesc.Format        = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = Device->GetDevice()->CreateUnorderedAccessView(DebugHitMapTexture, &UavDesc, &DebugHitMapUAV);
    check(SUCCEEDED(hr));

    // SRV (후처리 패스에서 오버레이 렌더링)
    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Format              = DXGI_FORMAT_R32G32B32A32_FLOAT;
    SrvDesc.Texture2D.MipLevels = 1;

    hr = Device->GetDevice()->CreateShaderResourceView(DebugHitMapTexture, &SrvDesc, &DebugHitMapSRV);
    check(SUCCEEDED(hr));
}

void FTileBasedLightCulling::UpdateLightCullingParamsCB(const FFrameContext& frameContext, bool bEnable25DCulling)
{
    if (!LightCullingParamsCB)
        return;

    FLightCullingParams Params = {};
    Params.ScreenSizeX = frameContext.ViewportWidth;
    Params.ScreenSizeY = frameContext.ViewportHeight;
    Params.TileSizeX        = TILE_SIZE;
    Params.TileSizeY        = TILE_SIZE;
    Params.Enable25DCulling = bEnable25DCulling ? 1u : 0u;
    Params.NearZ            = frameContext.NearClip;  // 카메라 상수에서 가져오도록 교체 권장
    Params.FarZ             = frameContext.FarClip;
    Params.NumLights        = (float)Lights.size();

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    HRESULT hr = Device->GetDeviceContext()->Map(LightCullingParamsCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(Mapped.pData, &Params, sizeof(Params));
        Device->GetDeviceContext()->Unmap(LightCullingParamsCB, 0);
    }
}
