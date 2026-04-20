#include "Render/Frame/FrameSharedResources.h"
#include "Materials/MaterialManager.h"

void FFrameSharedResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));

	// s0: LinearClamp (PostProcess, UI, 기본)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		InDevice->CreateSamplerState(&desc, &LinearClampSampler);
	}

	// s1: LinearWrap (메시 텍스처, 데칼)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		InDevice->CreateSamplerState(&desc, &LinearWrapSampler);
	}

	// s2: PointClamp (폰트, 깊이/스텐실 정밀 읽기)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		InDevice->CreateSamplerState(&desc, &PointClampSampler);
	}

	FMaterialManager::Get().Initialize(InDevice);
}

void FFrameSharedResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();

	if (LinearClampSampler) { LinearClampSampler->Release(); LinearClampSampler = nullptr; }
	if (LinearWrapSampler)  { LinearWrapSampler->Release();  LinearWrapSampler = nullptr; }
	if (PointClampSampler)  { PointClampSampler->Release();  PointClampSampler = nullptr; }
}

void FFrameSharedResources::BindSystemSamplers(ID3D11DeviceContext* Ctx)
{
	ID3D11SamplerState* Samplers[3] = { LinearClampSampler, LinearWrapSampler, PointClampSampler };
	Ctx->PSSetSamplers(0, 3, Samplers);
}
