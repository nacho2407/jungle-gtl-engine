#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include "Core/CoreMinimal.h"
#include <d3d11.h>

static constexpr uint32 ShadowAtlasResolution2D = 8192;
static constexpr int CUBE_FACE_COUNT = 6;
static constexpr int MAX_SHADOW_CUBES = 32;
static constexpr uint32 SHADOW_CUBE_SIZE = 512;

struct FShadowAtlasTile
{
    int32 TileX = 0;
    int32 TileY = 0;

    int32 PixelX = 0;
    int32 PixelY = 0;

    int32 Width = 0;
    int32 Height = 0;

    FVector4 ScaleOffset;
};

struct FShadowAtlasAllocator
{
public:
    static constexpr int32 AtlasSize = 8192;
    static constexpr int32 GridSize = 256;
    static constexpr int32 GridCount = AtlasSize / GridSize; // 32

    static constexpr int32 AtlasSizeTier[4] = { 256, 512, 1024, 2048 };
    static constexpr int32 TierCount = sizeof(AtlasSizeTier) / sizeof(AtlasSizeTier[0]);

private:
    bool Used[GridCount][GridCount] = {};

public:
    void Reset()
    {
        memset(Used, 0, sizeof(Used));
    }

    int32 QuantizeShadowSize(const int32 RequestTileSize) const
    {
        if (RequestTileSize <= 0)
        {
            return AtlasSizeTier[0];
        }

        for (int32 i = 0; i < TierCount; ++i)
        {
            if (RequestTileSize <= AtlasSizeTier[i])
            {
                return AtlasSizeTier[i];
            }
        }

        return AtlasSizeTier[TierCount - 1];
    }

	bool CanAllocate(int32 StartX, int32 StartY, int32 TileGridCount) const
	{
		for (int32 y = 0; y < TileGridCount; ++y)
		{
			for (int32 x = 0; x < TileGridCount; ++x)
			{
                if (Used[StartY + y][StartX + x])
                {
					return false;
				}
			}
		}
		return true;
    }

	void MarkUsed(int32 StartX, int32 StartY, int32 TileGridCount)
	{
		for (int32 y = 0; y < TileGridCount; ++y)
		{
			for (int32 x = 0; x < TileGridCount; ++x)
			{
				Used[StartY + y][StartX + x] = true;
			}
		}
    }

	bool AllocateTiled(int32& RequestTileSize, int32& OutTileX, int32& OutTileY)
	{
        RequestTileSize = QuantizeShadowSize(RequestTileSize);

        const int32 LocalTileSize = RequestTileSize;
        const int32 LocalGridCount = LocalTileSize / GridSize;

		for (int32 y = 0; y <= GridCount - LocalGridCount; ++y)
		{
            for (int32 x = 0; x <= GridCount - LocalGridCount; ++x)
            {
				if (CanAllocate(x, y, LocalGridCount)) {
					MarkUsed(x, y, LocalGridCount);
					OutTileX = x;
					OutTileY = y;
                    return true;
				}
            }
        }

        return false;
    }

	void FreeTile(int32 TileX, int32 TileY, int32 GridSize)
	{
        for (int32 y = 0; y < GridSize; ++y)
        {
            for (int32 x = 0; x < GridSize; ++x)
            {
                if (TileX + x >= 0 && TileX + x < GridCount && TileY + y >= 0 && TileY + y < GridCount)
                {
                    Used[TileY + y][TileX + x] = false;
                }
            }
		}
    }

    void FreeAllTiles()
    {
        for (int32 Y = 0; Y < GridCount; ++Y)
        {
            for (int32 X = 0; X < GridCount; ++X)
            {
                Used[Y][X] = false;
            }
        }
    }
};

struct FShadowAtlasCube
{
    TComPtr<ID3D11Texture2D> CubeShadowMap;
    TComPtr<ID3D11ShaderResourceView> CubeSRV;

    TComPtr<ID3D11DepthStencilView> CubeDSV[MAX_SHADOW_CUBES][CUBE_FACE_COUNT] = {};

    uint32 CurrentCubeCount = 0;

    void Initialize(ID3D11Device* Device)
    {
        if (Device == nullptr)
            return;
        // Cube Shadow Map Texture 생성
        D3D11_TEXTURE2D_DESC CubeShadowDesc = {};
        CubeShadowDesc.Width = SHADOW_CUBE_SIZE;
        CubeShadowDesc.Height = SHADOW_CUBE_SIZE;
        CubeShadowDesc.MipLevels = 1;
        CubeShadowDesc.ArraySize = 6 * MAX_SHADOW_CUBES; // Cube Map은 6개의 면으로 구성
        CubeShadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        CubeShadowDesc.SampleDesc.Count = 1;
        CubeShadowDesc.Usage = D3D11_USAGE_DEFAULT;
        CubeShadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        CubeShadowDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT hr = Device->CreateTexture2D(&CubeShadowDesc, nullptr, CubeShadowMap.ReleaseAndGetAddressOf());

        for (int CubeIndex = 0; CubeIndex < MAX_SHADOW_CUBES; ++CubeIndex)
        {
            for (int FaceIndex = 0; FaceIndex < CUBE_FACE_COUNT; ++FaceIndex)
            {
                // 각 면에 대한 DSV 생성
                D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
                DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;                       // Cube Map은 Texture2DArray로 처리
                DsvDesc.Texture2DArray.ArraySize = 1;                                             // 각 DSV는 하나의 면만 참조
                DsvDesc.Texture2DArray.FirstArraySlice = FaceIndex + CubeIndex * CUBE_FACE_COUNT; // 각 면에 대한 슬라이스 계산
                DsvDesc.Texture2DArray.MipSlice = 0;
                hr = Device->CreateDepthStencilView(CubeShadowMap.Get(), &DsvDesc, CubeDSV[CubeIndex][FaceIndex].ReleaseAndGetAddressOf());
            }
        }

        // Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY; // Cube Map은 TextureCubeArray로 처리
        SrvDesc.TextureCubeArray.MostDetailedMip = 0;
        SrvDesc.TextureCubeArray.MipLevels = 1;
        SrvDesc.TextureCubeArray.First2DArrayFace = 0;
        SrvDesc.TextureCubeArray.NumCubes = MAX_SHADOW_CUBES;
        hr = Device->CreateShaderResourceView(CubeShadowMap.Get(), &SrvDesc, CubeSRV.ReleaseAndGetAddressOf());
    }

	bool AllocateCube(int32& OutCubeIndex) {
		if (CurrentCubeCount < MAX_SHADOW_CUBES)
        {
            OutCubeIndex = CurrentCubeCount;
            ++CurrentCubeCount;
            return true;
		}
		return false;
	}

	void FreeCube(int32 CubeIndex) {
		if (CubeIndex >= 0 && CubeIndex < MAX_SHADOW_CUBES)
		{
			// 실제로는 DSV와 SRV를 재사용할 수 있도록 관리하는 로직이 필요하지만, 간단히 카운트만 감소시키는 방식으로 구현
			--CurrentCubeCount;
		}
	}

	void FreeAllCubes() {
		CurrentCubeCount = 0;
	}

    bool IsValid() const { return CubeShadowMap != nullptr && CubeDSV != nullptr && CubeSRV != nullptr; }
};

struct FShadowAtlas
{	
	// vsm 사용시 해당 포맷 DXGI_FORMAT_R32G32_FLOAT로 변경 필요
    TComPtr<ID3D11Texture2D> ShadowMapAtlas;
    TComPtr<ID3D11DepthStencilView> ShadowDSV;
    TComPtr<ID3D11ShaderResourceView> ShadowSRV;

    TComPtr<ID3D11Texture2D> VarianceShadowTexture;
    TComPtr<ID3D11RenderTargetView> VarianceShadowRTV;
    TComPtr<ID3D11ShaderResourceView> VarianceShadowSRV;
    TComPtr<ID3D11UnorderedAccessView> VarianceShadowUAV;

	// TODO: 타일의 크기가 정해져 있음, 이를 나중에 동적으로 조절할 수 있도록 개선해야 함
	// TODO: Tile 관리 방식 개선 필요. 현재는 간단히 bool 배열로 관리하지만, 더 효율적인 자료구조로 변경 고려 (예: 비트맵, 큐 등)

	void Initialize(ID3D11Device* Device)
    {
        if (Device == nullptr)
            return;

		// Atlas용 Depth Texture 생성
        D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
        ShadowMapDesc.Width = ShadowAtlasResolution2D;
        ShadowMapDesc.Height = ShadowAtlasResolution2D;
        ShadowMapDesc.MipLevels = 1;
        ShadowMapDesc.ArraySize = 1;
        ShadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        ShadowMapDesc.SampleDesc.Count = 1;
        ShadowMapDesc.Usage = D3D11_USAGE_DEFAULT;
        ShadowMapDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = Device->CreateTexture2D(&ShadowMapDesc, nullptr, ShadowMapAtlas.ReleaseAndGetAddressOf());

		// Depth Stencil View(DSV) 생성
        D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
        DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        DsvDesc.Flags = 0;
        DsvDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateDepthStencilView(ShadowMapAtlas.Get(), &DsvDesc, ShadowDSV.ReleaseAndGetAddressOf());

		// Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(ShadowMapAtlas.Get(), &SrvDesc, ShadowSRV.ReleaseAndGetAddressOf());

		// VSM용 Depth Texture 생성
        D3D11_TEXTURE2D_DESC VarianceTexDesc = {};
        VarianceTexDesc.Width = ShadowAtlasResolution2D;
        VarianceTexDesc.Height = ShadowAtlasResolution2D;
        VarianceTexDesc.MipLevels = 1;
        VarianceTexDesc.ArraySize = 1;
        VarianceTexDesc.Format = DXGI_FORMAT_R32G32_TYPELESS; // R32 -> 1차 모멘트, G32 -> 2차 모멘트
        VarianceTexDesc.SampleDesc.Count = 1;
        VarianceTexDesc.SampleDesc.Quality = 0;
        VarianceTexDesc.Usage = D3D11_USAGE_DEFAULT;
        VarianceTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        VarianceTexDesc.CPUAccessFlags = 0;
        VarianceTexDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&VarianceTexDesc, nullptr, VarianceShadowTexture.ReleaseAndGetAddressOf());

        // VSM용 Render Target View(RTV) 생성
        D3D11_RENDER_TARGET_VIEW_DESC VarianceRTVDesc = {};
        VarianceRTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        VarianceRTVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateRenderTargetView(VarianceShadowTexture.Get(), &VarianceRTVDesc, VarianceShadowRTV.ReleaseAndGetAddressOf());

        // VSM용 Shader Resource View(SRV) 생성
        D3D11_SHADER_RESOURCE_VIEW_DESC VarianceSRVDesc = {};
        VarianceSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        VarianceSRVDesc.Texture2D.MostDetailedMip = 0;
        VarianceSRVDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(VarianceShadowTexture.Get(), &VarianceSRVDesc, VarianceShadowSRV.ReleaseAndGetAddressOf());

        // VSM용 Unordered Access View(UAV) 생성
        D3D11_UNORDERED_ACCESS_VIEW_DESC VarianceUAVDesc = {};
        VarianceUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        VarianceUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        VarianceUAVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateUnorderedAccessView(VarianceShadowTexture.Get(), &VarianceUAVDesc, VarianceShadowUAV.ReleaseAndGetAddressOf());
	}

    bool IsValid() const { return ShadowMapAtlas != nullptr && ShadowDSV != nullptr && ShadowSRV != nullptr; }
    bool IsValidVSM() const { return VarianceShadowTexture != nullptr && VarianceShadowRTV != nullptr && VarianceShadowSRV != nullptr && VarianceShadowUAV != nullptr; }
};

class FShadowAtlasManager : public TSingleton<FShadowAtlasManager>
{
	friend class TSingleton<FShadowAtlasManager>;
public:
	void Initialize(ID3D11Device* InDevice);
    void VSMInitialize(ID3D11Device* Device);

    bool AllocateTile(int32 ResolutionScale, FShadowAtlasTile& OutTile);
    bool FreeTile(const int32& TileX, const int32& TileY, const int32& TileSize);
    void ClearTiles() { ShadowAllocator.FreeAllTiles(); }

    bool AllocateTileCube(int32& OutCubeIndex);
    bool FreeTileCube(const int32& CubeIndex);
    void ClearTilesCube() { ShadowCubeMapArray.FreeAllCubes(); }

    ID3D11DepthStencilView* GetDSV() const { return ShadowMapAtlas.ShadowDSV.Get(); }
    ID3D11ShaderResourceView* GetSRV() const { return ShadowMapAtlas.ShadowSRV.Get(); }
    ID3D11Texture2D* GetAtlas() const { return ShadowMapAtlas.ShadowMapAtlas.Get(); }

	
    ID3D11ShaderResourceView* GetCubeSRV(int32 index) const { return ShadowCubeMapArray.CubeSRV.Get(); }
    ID3D11Texture2D* GetCubeAtlas(int32 index) const { return ShadowCubeMapArray.CubeShadowMap.Get(); }
	ID3D11DepthStencilView* GetCubeDSV(int32 CubeIndex, int32 FaceIndex) const
    {
        if (CubeIndex >= MAX_SHADOW_CUBES || FaceIndex >= 6)
        {
            return nullptr;
        }
        return ShadowCubeMapArray.CubeDSV[CubeIndex][FaceIndex].Get();
    }

    TComPtr<ID3D11Device> Device;
    TComPtr<ID3D11Texture2D> ShadowMap;
    TComPtr<ID3D11DepthStencilView> ShadowDSV;
    TComPtr<ID3D11ShaderResourceView> ShadowSRV;

    TComPtr<ID3D11Texture2D> VarianceRTVShadowMap;       // 텍스처
    TComPtr<ID3D11RenderTargetView> VarianceShadowRTV;   // 쓰기용 (Shadow Pass)
    TComPtr<ID3D11ShaderResourceView> VarianceShadowSRV; // 읽기용 (Lighting Pass)

	float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };

private:
	// TComPtr<ID3D11Texture2D> VarianceDepthShadowMap;	 // DSV용 깊이 테스트
    // TComPtr<ID3D11DepthStencilView> VarianceShadowDSV;   // 깊이 테스트용 (별도)

	FShadowAtlas ShadowMapAtlas;
    FShadowAtlasAllocator ShadowAllocator;

	FShadowAtlasCube ShadowCubeMapArray;
};
