#include "BlitRenderer.h"

#include "Renderer/ShaderResource.h"
#include "Core/Paths.h"
#include "Viewport.h"

FBlitRenderer::~FBlitRenderer()
{
	Release();
}

void FBlitRenderer::Initialize(ID3D11Device* Device)
{
	if (!Device || bInitialized)
	{
		return;
	}

	std::wstring ShaderDir = FPaths::ShaderDir().wstring();

	auto VSResource = FShaderResource::GetOrCompile((ShaderDir + L"BlitVertexShader.hlsl").c_str(), "main", "vs_5_0");
	if (VSResource)
	{
		Device->CreateVertexShader(VSResource->GetBufferPointer(), VSResource->GetBufferSize(), nullptr, &BlitVS);
	}

	auto PSResource = FShaderResource::GetOrCompile((ShaderDir + L"BlitPixelShader.hlsl").c_str(), "main", "ps_5_0");
	if (PSResource)
	{
		Device->CreatePixelShader(PSResource->GetBufferPointer(), PSResource->GetBufferSize(), nullptr, &BlitPS);
	}

	// 포인트 샘플러
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SamplerDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD         = 0.0f;
	SamplerDesc.MaxLOD         = D3D11_FLOAT32_MAX;
	Device->CreateSamplerState(&SamplerDesc, &Sampler);

	// 깊이 테스트 없음
	D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
	DepthDesc.DepthEnable    = FALSE;
	DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthDesc.DepthFunc      = D3D11_COMPARISON_ALWAYS;
	Device->CreateDepthStencilState(&DepthDesc, &NoDepthState);

	// Scissor 활성화 래스터라이저
	D3D11_RASTERIZER_DESC RastDesc = {};
	RastDesc.FillMode        = D3D11_FILL_SOLID;
	RastDesc.CullMode        = D3D11_CULL_NONE;
	RastDesc.ScissorEnable   = TRUE;
	RastDesc.DepthClipEnable = TRUE;
	Device->CreateRasterizerState(&RastDesc, &RasterizerState);

	bInitialized = true;
}

void FBlitRenderer::Release()
{
	if (BlitVS)         { BlitVS->Release();          BlitVS          = nullptr; }
	if (BlitPS)         { BlitPS->Release();           BlitPS          = nullptr; }
	if (Sampler)        { Sampler->Release();          Sampler         = nullptr; }
	if (NoDepthState)   { NoDepthState->Release();     NoDepthState    = nullptr; }
	if (RasterizerState){ RasterizerState->Release();  RasterizerState = nullptr; }

	bInitialized = false;
}

void FBlitRenderer::BlitAll(ID3D11DeviceContext* Context, const TArray<FViewportEntry>& Entries)
{
	if (!bInitialized || !Context) return;

	Context->VSSetShader(BlitVS, nullptr, 0);
	Context->PSSetShader(BlitPS, nullptr, 0);
	Context->IASetInputLayout(nullptr);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->PSSetSamplers(0, 1, &Sampler);
	Context->OMSetDepthStencilState(NoDepthState, 0);
	Context->RSSetState(RasterizerState);

	for (const FViewportEntry& Entry : Entries)
	{
		if (!Entry.bActive || !Entry.Viewport) continue;

		const FRect& Rect = Entry.Viewport->GetRect();
		if (!Rect.IsValid()) continue;

		D3D11_VIEWPORT EntryVP = {};
		EntryVP.TopLeftX = static_cast<float>(Rect.X);
		EntryVP.TopLeftY = static_cast<float>(Rect.Y);
		EntryVP.Width = static_cast<float>(Rect.Width);
		EntryVP.Height = static_cast<float>(Rect.Height);
		EntryVP.MinDepth = 0.0f;
		EntryVP.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &EntryVP);

		D3D11_RECT ScissorRect;
		ScissorRect.left   = Rect.X;
		ScissorRect.top    = Rect.Y;
		ScissorRect.right  = Rect.X + Rect.Width;
		ScissorRect.bottom = Rect.Y + Rect.Height;
		Context->RSSetScissorRects(1, &ScissorRect);

		ID3D11ShaderResourceView* SRV = Entry.Viewport->GetSRV();
		Context->PSSetShaderResources(0, 1, &SRV);

		Context->Draw(6, 0);
	}

	// SRV 해제
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &NullSRV);
}
