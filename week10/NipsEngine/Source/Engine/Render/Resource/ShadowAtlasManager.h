#pragma once

#include "Core/Singleton.h"
#include "Render/Common/ComPtr.h"
#include "Core/CoreTypes.h"
#include <d3d11.h>

struct FShadowAtlas
{	
    TComPtr<ID3D11Texture2D> ShadowMap;
    TComPtr<ID3D11DepthStencilView> ShadowDSV;
    TComPtr<ID3D11ShaderResourceView> ShadowSRV;

    int32 TileSize = 1024;
    int32 TileCountX = 8;
    int32 TileCountY = 8;

	// TODO: 타일의 크기가 정해져 있음, 이를 나중에 동적으로 조절할 수 있도록 개선해야 함
	// TODO: Tile 관리 방식 개선 필요. 현재는 간단히 bool 배열로 관리하지만, 더 효율적인 자료구조로 변경 고려 (예: 비트맵, 큐 등)
    bool TileUsed[64] = {};

	void Initialize(ID3D11Device* Device)
    {
        if (Device == nullptr)
            return;

        D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
        ShadowMapDesc.Width = 8192;
        ShadowMapDesc.Height = 8192;
        ShadowMapDesc.MipLevels = 1;
        ShadowMapDesc.ArraySize = 1;
        ShadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        ShadowMapDesc.SampleDesc.Count = 1;
        ShadowMapDesc.Usage = D3D11_USAGE_DEFAULT;
        ShadowMapDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        Device->CreateTexture2D(&ShadowMapDesc, nullptr, &ShadowMap);

        D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
        DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        DsvDesc.Flags = 0;
        DsvDesc.Texture2D.MipSlice = 0;

        Device->CreateDepthStencilView(ShadowMap.Get(), &DsvDesc, &ShadowDSV);

        D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
        SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

        Device->CreateShaderResourceView(ShadowMap.Get(), &SrvDesc, &ShadowSRV);
	}
};

class FShadowAtlasManager : public TSingleton<FShadowAtlasManager>
{
	friend class TSingleton<FShadowAtlasManager>;
public:
	void Initialize(ID3D11Device* InDevice);

    bool AllocateTile(int32& OutTileX, int32& OutTileY);
    bool FreeTile(int32 TileX, int32 TileY);

	uint32 GetTileSize() const { return ShadowMapAtlas.TileSize; }

	TComPtr<ID3D11Device> Device;

	//TComPtr<ID3D11Texture2D> ShadowMap;
	//TComPtr<ID3D11DepthStencilView> ShadowDSV;
	//TComPtr<ID3D11ShaderResourceView> ShadowSRV;
	
	FShadowAtlas ShadowMapAtlas;
};
