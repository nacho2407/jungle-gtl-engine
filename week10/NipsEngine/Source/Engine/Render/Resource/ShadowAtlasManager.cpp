#include "ShadowAtlasManager.h"

void FShadowAtlasManager::Initialize(ID3D11Device* InDevice)
{
	if (InDevice == nullptr) return;

	ShadowMapAtlas.Initialize(InDevice);
}

bool FShadowAtlasManager::AllocateTile(FShadowAtlasTile& OutTile)
{
    const int32 TileSize = GetTileSize();

	int32 TileX, TileY;

    if (ShadowAllocator.AllocateTile(TileX, TileY))
    {
        OutTile.TileIndex = TileY * ShadowAllocator.TileCountX + TileX;

        OutTile.TileX = TileX;
        OutTile.TileY = TileY;

        OutTile.PixelX = TileX * TileSize;
        OutTile.PixelY = TileY * TileSize;

        OutTile.Width = TileSize;
        OutTile.Height = TileSize;

        const float AtlasW = static_cast<float>(GetAtlasWidth());
        const float AtlasH = static_cast<float>(GetAtlasHeight());
        const float Tile = static_cast<float>(TileSize);

        OutTile.ScaleOffset = FVector4(
            Tile / AtlasW,
            Tile / AtlasH,
            static_cast<float>(OutTile.PixelX) / AtlasW,
            static_cast<float>(OutTile.PixelY) / AtlasH);

        return true;
    }
	return false;
}



void FShadowAtlasManager::VSMInitialize(ID3D11Device* InDevice)
{
    if (InDevice == nullptr)
        return;

    Device = InDevice;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1024;
    texDesc.Height = 1024;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;

    // RTV용 텍스처
    texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    Device.Get()->CreateTexture2D(&texDesc, nullptr, &VarianceRTVShadowMap);

    //// DSV용 텍스처 - Format과 BindFlags만 바꿔서 재사용
    // texDesc.Format = DXGI_FORMAT_D32_FLOAT;
    // texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    // Device->CreateTexture2D(&texDesc, nullptr, &VarianceDepthShadowMap);


    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    Device.Get()->CreateRenderTargetView(VarianceRTVShadowMap.Get(), &rtvDesc, &VarianceShadowRTV);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    Device.Get()->CreateShaderResourceView(VarianceRTVShadowMap.Get(), &srvDesc, &VarianceShadowSRV);


    // D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    //   dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    //   dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    //   dsvDesc.Texture2D.MipSlice = 0;
    //   Device.Get()->CreateDepthStencilView(VarianceDepthShadowMap.Get(), &dsvDesc, &VarianceShadowDSV);
}

bool FShadowAtlasManager::FreeTile(const int32& TileIndex)
{
	if (TileIndex >= 0 && TileIndex < ShadowAllocator.TileCountX * ShadowAllocator.TileCountY)
	{
		int32 TileX = TileIndex % ShadowAllocator.TileCountX;
		int32 TileY = TileIndex / ShadowAllocator.TileCountX;
		ShadowAllocator.FreeTile(TileX, TileY);
		return true;
    }
    return false;
}
