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

void FShadowAtlasManager::VSMInitialize(ID3D11Device* InDevice)
{
    if (Device == nullptr)
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
    // DSV용 텍스처 - Format과 BindFlags만 바꿔서 재사용
    texDesc.Format = DXGI_FORMAT_D32_FLOAT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    Device->CreateTexture2D(&texDesc, nullptr, &VarianceDepthShadowMap);


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

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    Device.Get()->CreateDepthStencilView(VarianceDepthShadowMap.Get(), &dsvDesc, &VarianceShadowDSV);

}
