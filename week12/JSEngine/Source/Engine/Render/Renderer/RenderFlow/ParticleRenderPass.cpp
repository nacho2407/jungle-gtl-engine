#include "pch.h"
#include "ParticleRenderPass.h"

#include "Core/ResourceManager.h"
#include "Particle/ParticleModules.h"
#include "Particle/ParticleTypes.h"
#include "Render/SubUVBatcher.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Scene/RenderCommand.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/ShaderTypes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

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

	FVector GetParticleWorldLocation(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FBaseParticle& Particle)
	{
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformPosition(Particle.Location)
			: Particle.Location;
	}

	float GetParticleDepthKey(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FBaseParticle& Particle,
		const FVector& CameraPosition,
		const FVector& CameraForward)
	{
		const FVector Delta = GetParticleWorldLocation(ReplayData, ComponentToWorld, Particle) - CameraPosition;
		return FVector::DotProduct(Delta, CameraForward);
	}

	TArray<int32> BuildSortedActiveIndices(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FRenderBus& RenderBus)
	{
		TArray<int32> SortedIndices;
		SortedIndices.reserve(ReplayData.ActiveParticleCount);
		for (int32 ActiveIndex = 0; ActiveIndex < ReplayData.ActiveParticleCount; ++ActiveIndex)
		{
			if (ReplayData.GetParticleByActiveIndex(ActiveIndex) != nullptr)
			{
				SortedIndices.push_back(ActiveIndex);
			}
		}

		switch (ReplayData.SortMode)
		{
		case EParticleSortMode::ViewDepthBackToFront:
			std::sort(SortedIndices.begin(), SortedIndices.end(),
				[&ReplayData, &ComponentToWorld, &RenderBus](int32 A, int32 B)
				{
					const FBaseParticle* ParticleA = ReplayData.GetParticleByActiveIndex(A);
					const FBaseParticle* ParticleB = ReplayData.GetParticleByActiveIndex(B);
					return GetParticleDepthKey(ReplayData, ComponentToWorld, *ParticleA, RenderBus.GetCameraPosition(), RenderBus.GetCameraForward()) >
						GetParticleDepthKey(ReplayData, ComponentToWorld, *ParticleB, RenderBus.GetCameraPosition(), RenderBus.GetCameraForward());
				});
			break;
		case EParticleSortMode::ViewDepthFrontToBack:
			std::sort(SortedIndices.begin(), SortedIndices.end(),
				[&ReplayData, &ComponentToWorld, &RenderBus](int32 A, int32 B)
				{
					const FBaseParticle* ParticleA = ReplayData.GetParticleByActiveIndex(A);
					const FBaseParticle* ParticleB = ReplayData.GetParticleByActiveIndex(B);
					return GetParticleDepthKey(ReplayData, ComponentToWorld, *ParticleA, RenderBus.GetCameraPosition(), RenderBus.GetCameraForward()) <
						GetParticleDepthKey(ReplayData, ComponentToWorld, *ParticleB, RenderBus.GetCameraPosition(), RenderBus.GetCameraForward());
				});
			break;
		case EParticleSortMode::RelativeTime:
			std::sort(SortedIndices.begin(), SortedIndices.end(),
				[&ReplayData](int32 A, int32 B)
				{
					const FBaseParticle* ParticleA = ReplayData.GetParticleByActiveIndex(A);
					const FBaseParticle* ParticleB = ReplayData.GetParticleByActiveIndex(B);
					return ParticleA->RelativeTime > ParticleB->RelativeTime;
				});
			break;
		case EParticleSortMode::None:
		default:
			break;
		}

		return SortedIndices;
	}

	void AppendParticleSpriteInstance(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FBaseParticle& Particle,
		uint32 InstanceIndex,
		const FVector& CameraRight,
		const FVector& CameraUp,
		TArray<FParticleSpriteInstanceData>& Instances)
	{
		FVector WorldLocation = GetParticleWorldLocation(ReplayData, ComponentToWorld, Particle);
		const float Width = std::max(std::fabs(Particle.Size.X), 0.001f);
		const float Height = std::max(std::fabs(Particle.Size.Y), 0.001f);
		const float HalfW = Width * 0.5f;
		const float HalfH = Height * 0.5f;

		// TODO: FBaseParticle::Rotation 반영
		Instances.push_back({
			WorldLocation,
			CameraRight * HalfW,
			CameraUp * HalfH,
			Particle.Color
		});
	}

	const FSubUVParticlePayload* GetSubUVPayloadFromSnapshot(
		const FDynamicSpriteEmitterReplayDataBase& ReplayData,
		const FBaseParticle& Particle)
	{
		const int32 PayloadOffset = ReplayData.SubUVPayloadOffset;
		if (PayloadOffset < 0 || PayloadOffset + static_cast<int32>(sizeof(FSubUVParticlePayload)) > ReplayData.ParticleStride)
		{
			return nullptr;
		}

		const uint8* ParticleBytes = reinterpret_cast<const uint8*>(&Particle);
		return reinterpret_cast<const FSubUVParticlePayload*>(ParticleBytes + PayloadOffset);
	}

	bool TryAppendSubUVSprites(
		const FDynamicSpriteEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const TArray<int32>& SortedIndices,
		const FParticleConstants& ParticleConstants,
		FSubUVBatcher& SubUVBatcher)
	{
		if (ReplayData.SubUVPayloadOffset < 0 || ReplayData.SubUVTexture == nullptr)
		{
			return false;
		}

		const int32 Columns = std::max(ReplayData.SubUVColumns, 1);
		const int32 Rows = std::max(ReplayData.SubUVRows, 1);
		const int32 TotalFrames = Columns * Rows;
		if (TotalFrames <= 0)
		{
			return false;
		}

		for (int32 ActiveIndex : SortedIndices)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle == nullptr)
			{
				continue;
			}

			const FSubUVParticlePayload* Payload = GetSubUVPayloadFromSnapshot(ReplayData, *Particle);
			if (Payload == nullptr)
			{
				continue;
			}

			const uint32 FrameIndex = static_cast<uint32>(std::clamp(
				static_cast<int32>(Payload->ImageIndex + 0.5f),
				0,
				TotalFrames - 1));

			SubUVBatcher.AddSprite(
				ReplayData.SubUVTexture,
				GetParticleWorldLocation(ReplayData, ComponentToWorld, *Particle),
				ParticleConstants.CameraRight,
				ParticleConstants.CameraUp,
				Particle->Size,
				FrameIndex,
				static_cast<uint32>(Columns),
				static_cast<uint32>(Rows),
				1.0f,
				1.0f,
				Particle->Color);
		}

		return true;
	}
}

bool FParticleRenderPass::Initialize()
{
	return true;
}

bool FParticleRenderPass::Release()
{
	ClearBatch();
	QuadVertexBuffer.Reset();
	IndexBuffer.Reset();
	InstanceBuffer.Reset();
	MaxInstanceCount = 0;
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

	uint32 Strides[2] = { sizeof(FParticleSpriteQuadVertex), sizeof(FParticleSpriteInstanceData) };
	uint32 Offsets[2] = { 0, 0 };
	ID3D11Buffer* VertexBuffers[2] = { QuadVertexBuffer.Get(), nullptr };
	ID3D11Buffer* IndexBufferPtr = IndexBuffer.Get();

	for (const FRenderCommand& Cmd : Commands)
	{
		ClearBatch();

		const FDynamicEmitterReplayDataBase* ReplayData = Cmd.ParticleReplayData;
		if (ReplayData == nullptr || ReplayData->EmitterType != EDynamicEmitterType::Sprite)
		{
			continue;
		}

		const TArray<int32> SortedIndices = BuildSortedActiveIndices(
			*ReplayData,
			Cmd.Constants.Particle.ComponentToWorld,
			*Context->RenderBus);

		const FDynamicSpriteEmitterReplayDataBase* SpriteReplayData =
			static_cast<const FDynamicSpriteEmitterReplayDataBase*>(ReplayData);
		if (Context->SubUVBatcher != nullptr &&
			TryAppendSubUVSprites(
				*SpriteReplayData,
				Cmd.Constants.Particle.ComponentToWorld,
				SortedIndices,
				Cmd.Constants.Particle,
				*Context->SubUVBatcher))
		{
			continue;
		}

		for (int32 ActiveIndex : SortedIndices)
		{
			const FBaseParticle* Particle = ReplayData->GetParticleByActiveIndex(ActiveIndex);
			if (Particle != nullptr)
			{
				AppendParticleSpriteInstance(
					*ReplayData,
					Cmd.Constants.Particle.ComponentToWorld,
					*Particle,
					static_cast<uint32>(Instances.size()),
					Cmd.Constants.Particle.CameraRight,
					Cmd.Constants.Particle.CameraUp,
					Instances);
			}
		}

		if (Instances.empty())
		{
			continue;
		}

		if (!EnsureInstanceBuffer(Context, static_cast<uint32>(Instances.size())))
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(Context->DeviceContext->Map(InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			return false;
		}
		std::memcpy(Mapped.pData, Instances.data(), sizeof(FParticleSpriteInstanceData) * Instances.size());
		Context->DeviceContext->Unmap(InstanceBuffer.Get(), 0);

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

		VertexBuffers[1] = InstanceBuffer.Get();
		Context->DeviceContext->IASetVertexBuffers(0, 2, VertexBuffers, Strides, Offsets);
		Context->DeviceContext->IASetIndexBuffer(IndexBufferPtr, DXGI_FORMAT_R32_UINT, 0);
		Context->DeviceContext->DrawIndexedInstanced(6, static_cast<uint32>(Instances.size()), 0, 0, 0);
	}
	return true;
}

bool FParticleRenderPass::End(const FRenderPassContext* Context)
{
	(void)Context;
	ClearBatch();
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

bool FParticleRenderPass::EnsureInstanceBuffer(const FRenderPassContext* Context, uint32 InstanceCount)
{
	if (Context == nullptr || Context->Device == nullptr)
	{
		return false;
	}

	if (InstanceBuffer && InstanceCount <= MaxInstanceCount)
	{
		return true;
	}

	MaxInstanceCount = std::max(InstanceCount * 2u, 1u);
	InstanceBuffer.Reset();

	D3D11_BUFFER_DESC InstanceDesc = {};
	InstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	InstanceDesc.ByteWidth = sizeof(FParticleSpriteInstanceData) * MaxInstanceCount;
	InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(Context->Device->CreateBuffer(&InstanceDesc, nullptr, InstanceBuffer.ReleaseAndGetAddressOf()));
}

void FParticleRenderPass::ClearBatch()
{
	Instances.clear();
}
