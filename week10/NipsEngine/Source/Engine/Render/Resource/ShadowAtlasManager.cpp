#include "ShadowAtlasManager.h"

void FShadowAtlasManager::Initialize(ID3D11Device* InDevice)
{
	if (InDevice == nullptr) return;

	Device = InDevice;

	D3D11_TEXTURE2D_DESC ShadowMapDesc = {};
	ShadowMapDesc.Width = 1024;
	ShadowMapDesc.Height = 1024;
	ShadowMapDesc.MipLevels = 1;
	ShadowMapDesc.ArraySize = 1;
	ShadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	ShadowMapDesc.SampleDesc.Count = 1;
	ShadowMapDesc.Usage = D3D11_USAGE_DEFAULT;
	ShadowMapDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	Device.Get()->CreateTexture2D(&ShadowMapDesc, nullptr, &ShadowMap);

	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Flags = 0;
	DsvDesc.Texture2D.MipSlice = 0;

	Device.Get()->CreateDepthStencilView(ShadowMap.Get(), &DsvDesc, &ShadowDSV);

	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.MipLevels = 1;

	Device.Get()->CreateShaderResourceView(ShadowMap.Get(), &SrvDesc, &ShadowSRV);
}
