#include "RenderResources.h"

void FRenderResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	InDevice->CreateSamplerState(&sampDesc, &DefaultSampler);
}

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
	if (DefaultSampler) { DefaultSampler->Release(); DefaultSampler = nullptr; }
}
