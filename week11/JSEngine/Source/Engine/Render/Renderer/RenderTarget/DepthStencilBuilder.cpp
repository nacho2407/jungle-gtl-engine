#include "DepthStencilBuilder.h"

FDepthStencilBuilder& FDepthStencilBuilder::SetSize(uint32 InWidth, uint32 InHeight)
{
    Width = InWidth;
    Height = InHeight;

	return *this;
}

FDepthStencilBuilder& FDepthStencilBuilder::WithStencil()
{
    bUseStencil = true;

	return *this;
}

FDepthStencilBuilder& FDepthStencilBuilder::WithSRV()
{
    bCreateSRV = true;

	return *this;
}

FDepthStencilResource FDepthStencilBuilder::Build(ID3D11Device* Device)
{
    FDepthStencilResource DSR;

	D3D11_TEXTURE2D_DESC DepthStencilDesc = {};
    DepthStencilDesc.Width = Width;
    DepthStencilDesc.Height = Height;
    DepthStencilDesc.MipLevels = 1;
    DepthStencilDesc.ArraySize = 1;
    DepthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    DepthStencilDesc.SampleDesc.Count = 1;
    DepthStencilDesc.SampleDesc.Quality = 0;
    DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthStencilDesc.CPUAccessFlags = 0;
    DepthStencilDesc.MiscFlags = 0;

	DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (bCreateSRV)
		DepthStencilDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

	Device->CreateTexture2D(&DepthStencilDesc, nullptr, &DSR.Texture);
    
	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
    DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    DsvDesc.Flags = 0;
    DsvDesc.Texture2D.MipSlice = 0;

	Device->CreateDepthStencilView(DSR.Texture.Get(), &DsvDesc, &DSR.DSV);

	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = 1;

	Device->CreateShaderResourceView(DSR.Texture.Get(), &SrvDesc, &DSR.SRV);

	return DSR;
}
