#include "Renderer/ViewportCompositor.h"

#include "Core/Paths.h"
#include "Renderer/ShaderResource.h"

FViewportCompositor::~FViewportCompositor()
{
	Release();
}

bool FViewportCompositor::Initialize(ID3D11Device* Device)
{
	if (bInitialized)
	{
		return true;
	}

	if (!Device)
	{
		return false;
	}

	const std::wstring ShaderDir = FPaths::ShaderDir().wstring();

	auto BlitVSResource = FShaderResource::GetOrCompile((ShaderDir + L"BlitVertexShader.hlsl").c_str(), "main", "vs_5_0");
	if (!BlitVSResource || FAILED(Device->CreateVertexShader(BlitVSResource->GetBufferPointer(), BlitVSResource->GetBufferSize(), nullptr, &BlitVertexShader)))
	{
		Release();
		return false;
	}

	auto BlitPSResource = FShaderResource::GetOrCompile((ShaderDir + L"BlitPixelShader.hlsl").c_str(), "main", "ps_5_0");
	if (!BlitPSResource || FAILED(Device->CreatePixelShader(BlitPSResource->GetBufferPointer(), BlitPSResource->GetBufferSize(), nullptr, &BlitPixelShader)))
	{
		Release();
		return false;
	}

	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0.0f;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(Device->CreateSamplerState(&SamplerDesc, &PointSampler)))
	{
		Release();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
	DepthDesc.DepthEnable = FALSE;
	DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	if (FAILED(Device->CreateDepthStencilState(&DepthDesc, &NoDepthState)))
	{
		Release();
		return false;
	}

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.ScissorEnable = TRUE;
	RasterizerDesc.DepthClipEnable = TRUE;
	if (FAILED(Device->CreateRasterizerState(&RasterizerDesc, &ScissorRasterizerState)))
	{
		Release();
		return false;
	}

	bInitialized = true;
	return true;
}

void FViewportCompositor::Release()
{
	if (BlitVertexShader)
	{
		BlitVertexShader->Release();
		BlitVertexShader = nullptr;
	}
	if (BlitPixelShader)
	{
		BlitPixelShader->Release();
		BlitPixelShader = nullptr;
	}
	if (PointSampler)
	{
		PointSampler->Release();
		PointSampler = nullptr;
	}
	if (NoDepthState)
	{
		NoDepthState->Release();
		NoDepthState = nullptr;
	}
	if (ScissorRasterizerState)
	{
		ScissorRasterizerState->Release();
		ScissorRasterizerState = nullptr;
	}

	bInitialized = false;
}

bool FViewportCompositor::Compose(ID3D11DeviceContext* Context, const TArray<FViewportCompositeItem>& Items) const
{
	if (!bInitialized || !Context)
	{
		return false;
	}

	Context->VSSetShader(BlitVertexShader, nullptr, 0);
	Context->PSSetShader(BlitPixelShader, nullptr, 0);
	Context->IASetInputLayout(nullptr);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->PSSetSamplers(0, 1, &PointSampler);
	Context->OMSetDepthStencilState(NoDepthState, 0);
	Context->RSSetState(ScissorRasterizerState);

	for (const FViewportCompositeItem& Item : Items)
	{
		if (!Item.bVisible || !Item.SceneColorSRV || !Item.Rect.IsValid())
		{
			continue;
		}

		D3D11_VIEWPORT Viewport = {};
		Viewport.TopLeftX = static_cast<float>(Item.Rect.X);
		Viewport.TopLeftY = static_cast<float>(Item.Rect.Y);
		Viewport.Width = static_cast<float>(Item.Rect.Width);
		Viewport.Height = static_cast<float>(Item.Rect.Height);
		Viewport.MinDepth = 0.0f;
		Viewport.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &Viewport);

		D3D11_RECT ScissorRect = {};
		ScissorRect.left = Item.Rect.X;
		ScissorRect.top = Item.Rect.Y;
		ScissorRect.right = Item.Rect.X + Item.Rect.Width;
		ScissorRect.bottom = Item.Rect.Y + Item.Rect.Height;
		Context->RSSetScissorRects(1, &ScissorRect);

		ID3D11ShaderResourceView* SRV = Item.SceneColorSRV;
		Context->PSSetShaderResources(0, 1, &SRV);
		Context->Draw(6, 0);
	}

	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &NullSRV);
	return true;
}
