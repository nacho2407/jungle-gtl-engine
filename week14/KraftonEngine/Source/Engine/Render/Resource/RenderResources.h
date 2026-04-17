#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/ForwardLightData.h"

/*
	공용 Constant Buffer + System Sampler를 관리하는 구조체입니다.
	모든 커맨드가 공통으로 사용하는 Frame/PerObject CB만 소유합니다.
	타입별 CB(Gizmo, Editor, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

struct LightingResource
{
	ID3D11Buffer* LightBuffer = nullptr;
	ID3D11ShaderResourceView* LightBufferSRV = nullptr;
	uint32 MaxLightCount = 0;

	void Create(ID3D11Device* InDevice, uint32 MaxLightCount)
	{
		this->MaxLightCount = MaxLightCount;
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = sizeof(FLightInfo) * this->MaxLightCount;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		Desc.StructureByteStride = sizeof(FLightInfo);
		InDevice->CreateBuffer(&Desc, nullptr, &LightBuffer);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.NumElements = this->MaxLightCount;
		InDevice->CreateShaderResourceView(LightBuffer, &SRVDesc, &LightBufferSRV);
	}

	void Update(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, const TArray<FLightInfo>& LightInfos)
	{
		if (MaxLightCount < LightInfos.size())
		{
			Release();
			Create(InDevice, MaxLightCount * 2);
		}

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		InDeviceContext->Map(LightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
		memcpy(Mapped.pData, LightInfos.data(), sizeof(FLightInfo) * LightInfos.size());
		InDeviceContext->Unmap(LightBuffer, 0);
	}

	void Release()
	{
		if (LightBuffer) { LightBuffer->Release(); LightBuffer = nullptr; }
		if (LightBufferSRV) { LightBufferSRV->Release(); LightBufferSRV = nullptr; }
	}
};

struct FRenderResources
{
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame
	FConstantBuffer PerObjectConstantBuffer;	// b1 — ECBSlot::PerObject
	//Lighting
	FConstantBuffer LightingConstantBuffer;
	LightingResource ForwardLights;			// t8 

	// System Samplers — 프레임 시작 시 s0-s2에 영구 바인딩
	ID3D11SamplerState* LinearClampSampler = nullptr;	// s0
	ID3D11SamplerState* LinearWrapSampler = nullptr;	// s1
	ID3D11SamplerState* PointClampSampler = nullptr;	// s2

	void Create(ID3D11Device* InDevice);
	void Release();

	// s0-s2 시스템 샘플러 일괄 바인딩 (프레임 1회)
	void BindSystemSamplers(ID3D11DeviceContext* Ctx);
};
