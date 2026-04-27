#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include "Core/CoreMinimal.h"
#include <d3d11.h>

const uint32 ShadowAtlasResolution2D = 8192;

struct FShadowAtlasTile
{
    int32 TileIndex = -1;

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
	int32 TileSize = 1024;
	int32 TileCountX = 8;
	int32 TileCountY = 8;
	bool TileUsed[64] = {};

	bool AllocateTile(int32& OutTileX, int32& OutTileY)
	{
		for (int32 Y = 0; Y < TileCountY; ++Y)
		{
			for (int32 X = 0; X < TileCountX; ++X)
			{
				int32 Index = Y * TileCountX + X;
				if (!TileUsed[Index])
				{
					TileUsed[Index] = true;
					OutTileX = X;
					OutTileY = Y;
					return true;
				}
			}
		}
		return false; // No free tile found
	}

	void FreeTile(int32 TileX, int32 TileY)
	{
		if (TileX >= 0 && TileX < TileCountX && TileY >= 0 && TileY < TileCountY)
		{
			int32 Index = TileY * TileCountX + TileX;
			TileUsed[Index] = false;
		}
    }

    void FreeAllTiles()
    {
        for (bool& bUsed : TileUsed)
        {
            bUsed = false;
        }
    }
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

	// Blur 처리에 필요한 임시 Texture ,SRV , UAV 
	TComPtr<ID3D11Texture2D> BlurIntermediateTexture;       // 8192x8192, R32G32_FLOAT
    TComPtr<ID3D11ShaderResourceView> BlurIntermediateSRV;  // Vertical CS 읽기
    TComPtr<ID3D11UnorderedAccessView> BlurIntermediateUAV; // Horizontal CS 출력


	// TODO: 타일의 크기가 정해져 있음, 이를 나중에 동적으로 조절할 수 있도록 개선해야 함
	// TODO: Tile 관리 방식 개선 필요. 현재는 간단히 bool 배열로 관리하지만, 더 효율적인 자료구조로 변경 고려 (예: 비트맵, 큐 등)
	
	// ShadowMap, VSM에 사용하고자 하는 Resource를 하나로 통일해야함
	// ShadowManager의 ShadowMapAtlas의 자원을 모두 사용하도록 통일할 것.
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


		// Blur Intermediate Texture 생성 (가로 blur 임시 저장용)
        D3D11_TEXTURE2D_DESC BlurIntermediateDesc = {};
        BlurIntermediateDesc.Width = ShadowAtlasResolution2D;
        BlurIntermediateDesc.Height = ShadowAtlasResolution2D;
        BlurIntermediateDesc.MipLevels = 1;
        BlurIntermediateDesc.ArraySize = 1;
        BlurIntermediateDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurIntermediateDesc.SampleDesc.Count = 1;
        BlurIntermediateDesc.SampleDesc.Quality = 0;
        BlurIntermediateDesc.Usage = D3D11_USAGE_DEFAULT;
        BlurIntermediateDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        BlurIntermediateDesc.CPUAccessFlags = 0;
        BlurIntermediateDesc.MiscFlags = 0;

        hr = Device->CreateTexture2D(&BlurIntermediateDesc, nullptr, BlurIntermediateTexture.ReleaseAndGetAddressOf());

        // Blur Intermediate SRV 생성 (Vertical CS에서 읽기)
        D3D11_SHADER_RESOURCE_VIEW_DESC BlurSRVDesc = {};
        BlurSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        BlurSRVDesc.Texture2D.MostDetailedMip = 0;
        BlurSRVDesc.Texture2D.MipLevels = 1;

        hr = Device->CreateShaderResourceView(BlurIntermediateTexture.Get(), &BlurSRVDesc, BlurIntermediateSRV.ReleaseAndGetAddressOf());

        // Blur Intermediate UAV 생성 (Horizontal CS에서 쓰기)
        D3D11_UNORDERED_ACCESS_VIEW_DESC BlurUAVDesc = {};
        BlurUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        BlurUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        BlurUAVDesc.Texture2D.MipSlice = 0;

        hr = Device->CreateUnorderedAccessView(BlurIntermediateTexture.Get(), &BlurUAVDesc, BlurIntermediateUAV.ReleaseAndGetAddressOf());



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
	
	

    bool AllocateTile(FShadowAtlasTile& OutTile);
    bool FreeTile(const int32& TileIndex);
    void ClearTiles() { ShadowAllocator.FreeAllTiles(); }

	uint32 GetTileSize() const { return ShadowAllocator.TileSize; }
    int32 GetAtlasWidth() const { return ShadowAllocator.TileSize * ShadowAllocator.TileCountX; }
    int32 GetAtlasHeight() const { return ShadowAllocator.TileSize * ShadowAllocator.TileCountY; }

	///////////////////////////////// ShadowAtlas /////////////////////////////////////////////////////// 
	///////////////////////////////// ShadowAtlas /////////////////////////////////////////////////////// 
	///////////////////////////////// ShadowAtlas /////////////////////////////////////////////////////// 
	///////////////////////////////// ShadowAtlas /////////////////////////////////////////////////////// 
	// OpaquePass, BlurPass에서 꺼내 쓸 getter

	ID3D11RenderTargetView* GetVarianceRTV() const { return ShadowMapAtlas.VarianceShadowRTV.Get(); }
    ID3D11ShaderResourceView* GetVarianceSRV() const { return ShadowMapAtlas.VarianceShadowSRV.Get(); }
    ID3D11UnorderedAccessView* GetVarianceUAV() const { return ShadowMapAtlas.VarianceShadowUAV.Get(); }
    ID3D11ShaderResourceView* GetBlurSRV() const { return ShadowMapAtlas.BlurIntermediateSRV.Get(); }
    ID3D11UnorderedAccessView* GetBlurUAV() const { return ShadowMapAtlas.BlurIntermediateUAV.Get(); }


    ID3D11DepthStencilView* GetDSV() const { return ShadowMapAtlas.ShadowDSV.Get(); }
    ID3D11ShaderResourceView* GetSRV() const { return ShadowMapAtlas.ShadowSRV.Get(); }
    ID3D11Texture2D* GetAtlas() const { return ShadowMapAtlas.ShadowMapAtlas.Get(); }
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
};
