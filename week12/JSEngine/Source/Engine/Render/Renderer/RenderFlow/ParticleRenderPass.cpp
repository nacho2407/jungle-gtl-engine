#include "pch.h"
#include "ParticleRenderPass.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderTypes.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCommand.h"

#include <cstddef>

namespace
{
	struct FParticleSpriteQuadVertex
	{
		FVector Position;
		FVector2 TexCoord;
	};

	FShaderProgram* GetParticleSpriteShaderProgram()
	{
		static const FVertexLayoutDesc ParticleVertexLayout = {
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FParticleSpriteQuadVertex, Position)) },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FParticleSpriteQuadVertex, TexCoord)) },
				{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, static_cast<uint32>(offsetof(FParticleSpriteInstanceData, Center)), EVertexInputRate::PerInstance },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 1, static_cast<uint32>(offsetof(FParticleSpriteInstanceData, AxisX)), EVertexInputRate::PerInstance },
				{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT, 1, static_cast<uint32>(offsetof(FParticleSpriteInstanceData, AxisY)), EVertexInputRate::PerInstance },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<uint32>(offsetof(FParticleSpriteInstanceData, Color)), EVertexInputRate::PerInstance },
			},
			sizeof(FParticleSpriteQuadVertex)
		};

		FShaderStageKey VSKey;
		VSKey.FilePath = FShaderPaths::VFXParticle;
		VSKey.EntryPoint = "VS";
		VSKey.Target = "vs_5_0";

		FShaderStageKey PSKey;
		PSKey.FilePath = FShaderPaths::VFXParticle;
		PSKey.EntryPoint = "PS";
		PSKey.Target = "ps_5_0";

		return FResourceManager::Get().GetOrCreateShaderProgram(
			VSKey,
			PSKey,
			nullptr,
			nullptr,
			&ParticleVertexLayout);
	}
}

bool FParticleRenderPass::Initialize()
{
	return true;
}

bool FParticleRenderPass::Release()
{
	QuadVertexBuffer.Reset();
	IndexBuffer.Reset();
	return true;
}

bool FParticleRenderPass::Begin(const FRenderPassContext* Context)
{
	if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
	{
		OutSRV = PrevPassSRV;
		OutRTV = PrevPassRTV;
		return true;
	}

	ID3D11RenderTargetView* RTV = PrevPassRTV;
	ID3D11DepthStencilView* DSV = Context->RenderTargets->DepthStencilView;
	Context->DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
	Context->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	OutSRV = PrevPassSRV;
	OutRTV = PrevPassRTV;
	return true;
}

bool FParticleRenderPass::DrawCommand(const FRenderPassContext* Context)
{
	if (IsDebugViewMode(Context->RenderBus->GetViewMode()))
	{
		return true;
	}

	const TArray<FRenderCommand>& Commands = Context->RenderBus->GetCommands(ERenderPass::Particle);
	if (Commands.empty())
	{
		return true;
	}

	if (!EnsureQuadBuffers(Context))
	{
		return false;
	}

	FShaderProgram* Program = GetParticleSpriteShaderProgram();
	if (Program == nullptr)
	{
		return false;
	}

	Program->Bind(Context->DeviceContext);

	ID3D11DepthStencilState* DepthState = FResourceManager::Get().GetOrCreateDepthStencilState(EDepthStencilType::DepthReadOnly);
	ID3D11RasterizerState* RasterizerState = FResourceManager::Get().GetOrCreateRasterizerState(
		Context->RenderBus->GetViewMode() == EViewMode::Wireframe ? ERasterizerType::WireFrame : ERasterizerType::SolidBackCull);
	ID3D11SamplerState* Sampler = FResourceManager::Get().GetOrCreateSamplerState(ESamplerType::EST_Linear);
	Context->DeviceContext->OMSetDepthStencilState(DepthState, 0);
	Context->DeviceContext->RSSetState(RasterizerState);
	Context->DeviceContext->PSSetSamplers(0, 1, &Sampler);

	ID3D11Buffer* VertexBuffers[2] = { QuadVertexBuffer.Get(), nullptr };
	ID3D11Buffer* IndexBufferPtr = IndexBuffer.Get();

	for (const FRenderCommand& Cmd : Commands)
	{
		if (!Cmd.InstanceBufferView.IsValid())
		{
			continue;
		}

		ID3D11ShaderResourceView* DefaultDiffuseSRV = FResourceManager::Get().GetDefaultWhiteSRV();
		Context->DeviceContext->PSSetShaderResources(0, 1, &DefaultDiffuseSRV);
		if (Cmd.Material != nullptr)
		{
			Cmd.Material->BindParameters(Context->DeviceContext, Program->PS);
		}
		ID3D11BlendState* BlendState = Cmd.Material != nullptr
			? FResourceManager::Get().GetOrCreateBlendState(Cmd.Material->GetBlendStateDesc())
			: FResourceManager::Get().GetOrCreateBlendState(EBlendType::AlphaBlend);
		Context->DeviceContext->OMSetBlendState(BlendState, nullptr, 0xFFFFFFFF);

		uint32 Strides[2] = { sizeof(FParticleSpriteQuadVertex), Cmd.InstanceBufferView.Stride };
		uint32 Offsets[2] = { 0, Cmd.InstanceBufferView.Offset };
		VertexBuffers[1] = Cmd.InstanceBufferView.Buffer;
		Context->DeviceContext->IASetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
		Context->DeviceContext->IASetIndexBuffer(IndexBufferPtr, DXGI_FORMAT_R32_UINT, 0);
		Context->DeviceContext->DrawIndexedInstanced(6, Cmd.InstanceBufferView.InstanceCount, 0, 0, 0);
	}

	return true;
}

bool FParticleRenderPass::End(const FRenderPassContext* Context)
{
	(void)Context;
	return true;
}

bool FParticleRenderPass::EnsureQuadBuffers(const FRenderPassContext* Context)
{
	if (Context == nullptr || Context->Device == nullptr)
	{
		return false;
	}

	if (QuadVertexBuffer && IndexBuffer)
	{
		return true;
	}

	static const FParticleSpriteQuadVertex QuadVertices[] = {
		{ FVector(-1.0f,  1.0f, 0.0f), FVector2(0.0f, 0.0f) },
		{ FVector( 1.0f,  1.0f, 0.0f), FVector2(1.0f, 0.0f) },
		{ FVector(-1.0f, -1.0f, 0.0f), FVector2(0.0f, 1.0f) },
		{ FVector( 1.0f, -1.0f, 0.0f), FVector2(1.0f, 1.0f) },
	};
	static const uint32 QuadIndices[] = { 0, 1, 2, 1, 3, 2 };

	QuadVertexBuffer.Reset();
	IndexBuffer.Reset();

	D3D11_BUFFER_DESC VertexDesc = {};
	VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexDesc.ByteWidth = sizeof(QuadVertices);
	VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA VertexData = {};
	VertexData.pSysMem = QuadVertices;
	if (FAILED(Context->Device->CreateBuffer(&VertexDesc, &VertexData, QuadVertexBuffer.ReleaseAndGetAddressOf())))
	{
		return false;
	}

	D3D11_BUFFER_DESC IndexDesc = {};
	IndexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IndexDesc.ByteWidth = sizeof(QuadIndices);
	IndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA IndexData = {};
	IndexData.pSysMem = QuadIndices;
	if (FAILED(Context->Device->CreateBuffer(&IndexDesc, &IndexData, IndexBuffer.ReleaseAndGetAddressOf())))
	{
		QuadVertexBuffer.Reset();
		return false;
	}

	return true;
}
