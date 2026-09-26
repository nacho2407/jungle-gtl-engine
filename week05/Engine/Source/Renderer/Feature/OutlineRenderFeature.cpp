#include "Renderer/Feature/OutlineRenderFeature.h"

#include "Core/Paths.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/ShaderResource.h"

namespace
{
	struct FOutlinePostConstantBuffer
	{
		FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
		float OutlineThickness = 2.0f;
		float OutlineThreshold = 0.1f;
		float Padding[2] = {};
	};

	bool GetRenderTargetSize(ID3D11RenderTargetView* RenderTargetView, uint32& OutWidth, uint32& OutHeight)
	{
		if (!RenderTargetView)
		{
			return false;
		}

		ID3D11Resource* Resource = nullptr;
		RenderTargetView->GetResource(&Resource);
		if (!Resource)
		{
			return false;
		}

		ID3D11Texture2D* Texture = nullptr;
		const HRESULT Hr = Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture));
		Resource->Release();
		if (FAILED(Hr) || !Texture)
		{
			return false;
		}

		D3D11_TEXTURE2D_DESC Desc = {};
		Texture->GetDesc(&Desc);
		Texture->Release();

		OutWidth = Desc.Width;
		OutHeight = Desc.Height;
		return true;
	}
}

FOutlineRenderFeature::~FOutlineRenderFeature()
{
	Release();
}

bool FOutlineRenderFeature::Initialize(FRenderer& Renderer)
{
	ID3D11Device* Device = Renderer.GetDevice();
	if (!Device)
	{
		return false;
	}

	if (!OutlinePostConstantBuffer)
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.ByteWidth = sizeof(FOutlinePostConstantBuffer);
		if (FAILED(Device->CreateBuffer(&Desc, nullptr, &OutlinePostConstantBuffer)))
		{
			return false;
		}
	}

	if (!StencilWriteState)
	{
		D3D11_DEPTH_STENCIL_DESC WriteDesc = {};
		WriteDesc.DepthEnable = FALSE;
		WriteDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		WriteDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		WriteDesc.StencilEnable = TRUE;
		WriteDesc.StencilReadMask = 0xFF;
		WriteDesc.StencilWriteMask = 0xFF;
		WriteDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		WriteDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		WriteDesc.BackFace = WriteDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&WriteDesc, &StencilWriteState)))
		{
			return false;
		}
	}

	if (!StencilEqualState)
	{
		D3D11_DEPTH_STENCIL_DESC EqualDesc = {};
		EqualDesc.DepthEnable = FALSE;
		EqualDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		EqualDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		EqualDesc.StencilEnable = TRUE;
		EqualDesc.StencilReadMask = 0xFF;
		EqualDesc.StencilWriteMask = 0x00;
		EqualDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		EqualDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		EqualDesc.BackFace = EqualDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&EqualDesc, &StencilEqualState)))
		{
			return false;
		}
	}

	if (!StencilNotEqualState)
	{
		D3D11_DEPTH_STENCIL_DESC NotEqualDesc = {};
		NotEqualDesc.DepthEnable = FALSE;
		NotEqualDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		NotEqualDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		NotEqualDesc.StencilEnable = TRUE;
		NotEqualDesc.StencilReadMask = 0xFF;
		NotEqualDesc.StencilWriteMask = 0x00;
		NotEqualDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		NotEqualDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		NotEqualDesc.BackFace = NotEqualDesc.FrontFace;
		if (FAILED(Device->CreateDepthStencilState(&NotEqualDesc, &StencilNotEqualState)))
		{
			return false;
		}
	}

	if (!OutlineBlendState)
	{
		D3D11_BLEND_DESC BlendDesc = {};
		BlendDesc.RenderTarget[0].BlendEnable = TRUE;
		BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(Device->CreateBlendState(&BlendDesc, &OutlineBlendState)))
		{
			return false;
		}
	}

	if (!OutlineRasterizerState)
	{
		D3D11_RASTERIZER_DESC RasterDesc = {};
		RasterDesc.FillMode = D3D11_FILL_SOLID;
		RasterDesc.CullMode = D3D11_CULL_NONE;
		RasterDesc.DepthClipEnable = TRUE;
		if (FAILED(Device->CreateRasterizerState(&RasterDesc, &OutlineRasterizerState)))
		{
			return false;
		}
	}

	if (!OutlineSampler)
	{
		D3D11_SAMPLER_DESC SamplerDesc = {};
		SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		SamplerDesc.MinLOD = 0;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		if (FAILED(Device->CreateSamplerState(&SamplerDesc, &OutlineSampler)))
		{
			return false;
		}
	}

	const std::wstring ShaderDir = FPaths::ShaderDir();
	if (!OutlinePostVS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlinePostVertexShader.hlsl").c_str(), "main", "vs_5_0");
		if (!Resource || FAILED(Device->CreateVertexShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlinePostVS)))
		{
			return false;
		}
	}

	if (!OutlineMaskPS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlineMaskPixelShader.hlsl").c_str(), "main", "ps_5_0");
		if (!Resource || FAILED(Device->CreatePixelShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlineMaskPS)))
		{
			return false;
		}
	}

	if (!OutlineSobelPS)
	{
		auto Resource = FShaderResource::GetOrCompile((ShaderDir + L"OutlineSobelPixelShader.hlsl").c_str(), "main", "ps_5_0");
		if (!Resource || FAILED(Device->CreatePixelShader(Resource->GetBufferPointer(), Resource->GetBufferSize(), nullptr, &OutlineSobelPS)))
		{
			return false;
		}
	}

	return true;
}

bool FOutlineRenderFeature::EnsureOutlineMaskResources(FRenderer& Renderer, uint32 Width, uint32 Height)
{
	if (OutlineMaskTexture && OutlineMaskRTV && OutlineMaskSRV &&
		OutlineMaskWidth == Width && OutlineMaskHeight == Height)
	{
		return true;
	}

	ReleaseOutlineMaskResources();

	ID3D11Device* Device = Renderer.GetDevice();
	if (!Device)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&Desc, nullptr, &OutlineMaskTexture)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	if (FAILED(Device->CreateRenderTargetView(OutlineMaskTexture, nullptr, &OutlineMaskRTV)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	if (FAILED(Device->CreateShaderResourceView(OutlineMaskTexture, nullptr, &OutlineMaskSRV)))
	{
		ReleaseOutlineMaskResources();
		return false;
	}

	OutlineMaskWidth = Width;
	OutlineMaskHeight = Height;
	return true;
}

void FOutlineRenderFeature::ReleaseOutlineMaskResources()
{
	if (OutlineMaskSRV)
	{
		OutlineMaskSRV->Release();
		OutlineMaskSRV = nullptr;
	}
	if (OutlineMaskRTV)
	{
		OutlineMaskRTV->Release();
		OutlineMaskRTV = nullptr;
	}
	if (OutlineMaskTexture)
	{
		OutlineMaskTexture->Release();
		OutlineMaskTexture = nullptr;
	}

	OutlineMaskWidth = 0;
	OutlineMaskHeight = 0;
}

void FOutlineRenderFeature::UpdateOutlinePostConstantBuffer(
	FRenderer& Renderer,
	const FVector4& OutlineColor,
	float OutlineThickness,
	float OutlineThreshold)
{
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!OutlinePostConstantBuffer || !DeviceContext)
	{
		return;
	}

	FOutlinePostConstantBuffer CBData = {};
	CBData.OutlineColor = OutlineColor;
	CBData.OutlineThickness = OutlineThickness;
	CBData.OutlineThreshold = OutlineThreshold;

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (SUCCEEDED(DeviceContext->Map(OutlinePostConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &CBData, sizeof(CBData));
		DeviceContext->Unmap(OutlinePostConstantBuffer, 0);
	}

	ID3D11Buffer* Buffer = OutlinePostConstantBuffer;
	DeviceContext->PSSetConstantBuffers(0, 1, &Buffer);
}

bool FOutlineRenderFeature::Render(FRenderer& Renderer, const FOutlineRenderRequest& Request)
{
	if (!Request.bEnabled || Request.Items.empty() || !Initialize(Renderer))
	{
		return true;
	}

	ID3D11Device* Device = Renderer.GetDevice();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	if (!Device || !DeviceContext)
	{
		return false;
	}

	ID3D11RenderTargetView* BoundRTV = nullptr;
	ID3D11DepthStencilView* BoundDSV = nullptr;
	DeviceContext->OMGetRenderTargets(1, &BoundRTV, &BoundDSV);
	if (!BoundRTV || !BoundDSV)
	{
		if (BoundRTV) BoundRTV->Release();
		if (BoundDSV) BoundDSV->Release();
		return false;
	}

	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;
	if (!GetRenderTargetSize(BoundRTV, TargetWidth, TargetHeight) || !EnsureOutlineMaskResources(Renderer, TargetWidth, TargetHeight))
	{
		BoundRTV->Release();
		BoundDSV->Release();
		return false;
	}

	constexpr float ClearColor[4] = { 0.f, 0.f, 0.f, 0.f };
	constexpr float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11Buffer* NullCB = nullptr;

	DeviceContext->ClearDepthStencilView(BoundDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);
	Renderer.SetConstantBuffers();
	Renderer.ShaderManager.Bind(DeviceContext);
	DeviceContext->PSSetShader(nullptr, nullptr, 0);
	DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	DeviceContext->RSSetState(nullptr);
	DeviceContext->OMSetRenderTargets(0, nullptr, BoundDSV);
	DeviceContext->OMSetDepthStencilState(StencilWriteState, 1);

	for (const FOutlineRenderItem& Item : Request.Items)
	{
		if (!Item.Mesh || !Item.Mesh->UpdateVertexAndIndexBuffer(Device, DeviceContext))
		{
			continue;
		}

		Item.Mesh->Bind(DeviceContext);
		if (Item.Mesh->Topology != EMeshTopology::EMT_Undefined)
		{
			DeviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)Item.Mesh->Topology);
		}

		Renderer.UpdateObjectConstantBuffer(Item.WorldMatrix);
		if (!Item.Mesh->Indices.empty())
		{
			DeviceContext->DrawIndexed(static_cast<UINT>(Item.Mesh->Indices.size()), 0, 0);
		}
		else
		{
			DeviceContext->Draw(static_cast<UINT>(Item.Mesh->Vertices.size()), 0);
		}
	}

	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
	DeviceContext->OMSetRenderTargets(1, &OutlineMaskRTV, BoundDSV);
	DeviceContext->ClearRenderTargetView(OutlineMaskRTV, ClearColor);
	DeviceContext->OMSetDepthStencilState(StencilEqualState, 1);
	DeviceContext->RSSetState(OutlineRasterizerState);
	DeviceContext->IASetInputLayout(nullptr);
	DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->VSSetShader(OutlinePostVS, nullptr, 0);
	DeviceContext->PSSetShader(OutlineMaskPS, nullptr, 0);
	DeviceContext->Draw(3, 0);

	DeviceContext->OMSetRenderTargets(1, &BoundRTV, BoundDSV);
	DeviceContext->OMSetDepthStencilState(StencilNotEqualState, 1);
	DeviceContext->OMSetBlendState(OutlineBlendState, BlendFactor, 0xFFFFFFFF);
	DeviceContext->VSSetShader(OutlinePostVS, nullptr, 0);
	DeviceContext->PSSetShader(OutlineSobelPS, nullptr, 0);
	UpdateOutlinePostConstantBuffer(Renderer, FVector4(1.0f, 0.5f, 0.0f, 1.0f), 2.0f, 0.1f);
	DeviceContext->PSSetShaderResources(0, 1, &OutlineMaskSRV);
	DeviceContext->PSSetSamplers(0, 1, &OutlineSampler);
	DeviceContext->Draw(3, 0);

	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
	DeviceContext->PSSetConstantBuffers(0, 1, &NullCB);
	DeviceContext->ClearDepthStencilView(BoundDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);
	DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	DeviceContext->RSSetState(nullptr);
	Renderer.ShaderManager.Bind(DeviceContext);
	Renderer.SetConstantBuffers();
	Renderer.GetRenderStateManager()->RebindState();

	BoundRTV->Release();
	BoundDSV->Release();
	return true;
}

void FOutlineRenderFeature::Release()
{
	if (OutlinePostConstantBuffer)
	{
		OutlinePostConstantBuffer->Release();
		OutlinePostConstantBuffer = nullptr;
	}
	if (StencilWriteState)
	{
		StencilWriteState->Release();
		StencilWriteState = nullptr;
	}
	if (StencilEqualState)
	{
		StencilEqualState->Release();
		StencilEqualState = nullptr;
	}
	if (StencilNotEqualState)
	{
		StencilNotEqualState->Release();
		StencilNotEqualState = nullptr;
	}
	if (OutlineBlendState)
	{
		OutlineBlendState->Release();
		OutlineBlendState = nullptr;
	}
	if (OutlineRasterizerState)
	{
		OutlineRasterizerState->Release();
		OutlineRasterizerState = nullptr;
	}
	if (OutlineSampler)
	{
		OutlineSampler->Release();
		OutlineSampler = nullptr;
	}
	if (OutlinePostVS)
	{
		OutlinePostVS->Release();
		OutlinePostVS = nullptr;
	}
	if (OutlineMaskPS)
	{
		OutlineMaskPS->Release();
		OutlineMaskPS = nullptr;
	}
	if (OutlineSobelPS)
	{
		OutlineSobelPS->Release();
		OutlineSobelPS = nullptr;
	}

	ReleaseOutlineMaskResources();
}
