#include "RenderResources.h"
#include "Materials/MaterialManager.h"

void FRenderResources::Create(ID3D11Device* InDevice)
{
	FrameBuffer.Create(InDevice, sizeof(FFrameConstants));
	PerObjectConstantBuffer.Create(InDevice, sizeof(FPerObjectConstants));
	GlobalLightBuffer.Create(InDevice, sizeof(FGlobalLightConstants));

	// s0: LinearClamp (PostProcess, UI, 기본)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
		desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
		desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
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

void FRenderResources::Release()
{
	FrameBuffer.Release();
	PerObjectConstantBuffer.Release();
	GlobalLightBuffer.Release();

	if (LocalLightSRV)    { LocalLightSRV->Release();    LocalLightSRV    = nullptr; }
	if (LocalLightBuffer) { LocalLightBuffer->Release(); LocalLightBuffer = nullptr; }
	LocalLightCapacity = 0;

	if (LinearClampSampler) { LinearClampSampler->Release(); LinearClampSampler = nullptr; }
	if (LinearWrapSampler)  { LinearWrapSampler->Release();  LinearWrapSampler  = nullptr; }
	if (PointClampSampler)  { PointClampSampler->Release();  PointClampSampler  = nullptr; }
}

// Local Lights (SpotLight, PointLight) 정보가 담긴 StructuredBuffer을 t6 슬롯에 업로드합니다.
void FRenderResources::UpdateLocalLights(ID3D11Device* Device, ID3D11DeviceContext* Context, const TArray<FLocalLightInfo>& Lights)
{
	const uint32 Count = static_cast<uint32>(Lights.size());

	// 현재 버퍼 용량이 부족하면 해제 후 재생성
	if (Count > LocalLightCapacity)
	{
		if (LocalLightSRV)    { LocalLightSRV->Release();    LocalLightSRV    = nullptr; }
		if (LocalLightBuffer) { LocalLightBuffer->Release(); LocalLightBuffer = nullptr; }

		const uint32 NewCapacity = Count < 8u ? 8u : Count;

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth           = sizeof(FLocalLightInfo) * NewCapacity;
		Desc.Usage               = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = sizeof(FLocalLightInfo);
		Device->CreateBuffer(&Desc, nullptr, &LocalLightBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format              = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements  = NewCapacity;
		Device->CreateShaderResourceView(LocalLightBuffer, &SRVDesc, &LocalLightSRV);

		LocalLightCapacity = NewCapacity;
	}

	// 버퍼에 라이트 데이터 업로드
	if (LocalLightBuffer && Count > 0)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (SUCCEEDED(Context->Map(LocalLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			memcpy(Mapped.pData, Lights.data(), sizeof(FLocalLightInfo) * Count);
			Context->Unmap(LocalLightBuffer, 0);
		}
	}
}

void FRenderResources::BindSystemSamplers(ID3D11DeviceContext* Ctx)
{
	ID3D11SamplerState* Samplers[3] = { LinearClampSampler, LinearWrapSampler, PointClampSampler };
	Ctx->PSSetSamplers(0, 3, Samplers);
}
