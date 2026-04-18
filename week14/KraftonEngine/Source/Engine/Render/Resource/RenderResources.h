#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Pipeline/RenderConstants.h"
#include "Render/Pipeline/ForwardLightData.h"

#include "Render/Resource/RasterizerStateManager.h"
#include "Render/Resource/DepthStencilStateManager.h"
#include "Render/Resource/BlendStateManager.h"
#include "Render/Resource/SamplerStateManager.h"

/*
	시스템 레벨 GPU 리소스를 관리하는 구조체입니다.
	프레임 공용 CB (Frame, Lighting), 라이트 StructuredBuffer,
	렌더 상태 오브젝트(DSS/Blend/Rasterizer/Sampler),
	시스템 텍스처 언바인딩(t16-t19)을 소유합니다.
	셰이더별 CB(Gizmo, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

class FD3DDevice;
class FScene;
struct FFrameContext;

struct FLightingResource
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

struct FSystemResources
{
	// --- Frame CB (b0) ---
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame

	// --- Lighting ---
	FConstantBuffer LightingConstantBuffer;		// b4 — ECBSlot::Lighting
	FLightingResource ForwardLights;			// t8 — ELightTexSlot::AllLights

	// --- Render State Managers ---
	FRasterizerStateManager RasterizerStateManager;
	FDepthStencilStateManager DepthStencilStateManager;
	FBlendStateManager BlendStateManager;
	FSamplerStateManager SamplerStateManager;		// s0-s2

	void Create(ID3D11Device* InDevice);
	void Release();

	// 렌더 상태 전환
	void SetDepthStencilState(FD3DDevice& Device, EDepthStencilState InState);
	void SetBlendState(FD3DDevice& Device, EBlendState InState);
	void SetRasterizerState(FD3DDevice& Device, ERasterizerState InState);

	// 리사이즈 시 렌더 상태 캐시 무효화
	void ResetRenderStateCache();

	// 프레임 공용 CB 업데이트 + 바인딩 (b0)
	void UpdateFrameBuffer(FD3DDevice& Device, const FFrameContext& Frame);

	// 라이팅 CB + StructuredBuffer 업데이트 + 바인딩 (b4, t8)
	void UpdateLightBuffer(FD3DDevice& Device, const FScene& Scene);

	// s0-s2 시스템 샘플러 일괄 바인딩 (프레임 1회)
	void BindSystemSamplers(FD3DDevice& Device);

	// 시스템 텍스처 슬롯 언바인딩 (t16-t19)
	void UnbindSystemTextures(FD3DDevice& Device);
};
