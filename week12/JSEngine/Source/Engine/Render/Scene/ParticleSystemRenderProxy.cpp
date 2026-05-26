#include "ParticleSystemRenderProxy.h"

#include "Particle/ParticleSystemComponent.h"
#include "Particle/ParticleTypes.h"
#include "Render/Scene/RenderBus.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace
{
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
					return GetParticleDepthKey(
						ReplayData,
						ComponentToWorld,
						*ParticleA,
						RenderBus.GetCameraPosition(),
						RenderBus.GetCameraForward()) >
						GetParticleDepthKey(
							ReplayData,
							ComponentToWorld,
							*ParticleB,
							RenderBus.GetCameraPosition(),
							RenderBus.GetCameraForward());
				});
			break;
		case EParticleSortMode::ViewDepthFrontToBack:
			std::sort(SortedIndices.begin(), SortedIndices.end(),
				[&ReplayData, &ComponentToWorld, &RenderBus](int32 A, int32 B)
				{
					const FBaseParticle* ParticleA = ReplayData.GetParticleByActiveIndex(A);
					const FBaseParticle* ParticleB = ReplayData.GetParticleByActiveIndex(B);
					return GetParticleDepthKey(
						ReplayData,
						ComponentToWorld,
						*ParticleA,
						RenderBus.GetCameraPosition(),
						RenderBus.GetCameraForward()) <
						GetParticleDepthKey(
							ReplayData,
							ComponentToWorld,
							*ParticleB,
							RenderBus.GetCameraPosition(),
							RenderBus.GetCameraForward());
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
		const FVector& CameraRight,
		const FVector& CameraUp,
		TArray<FParticleSpriteInstanceData>& Instances)
	{
		const FVector WorldLocation = GetParticleWorldLocation(ReplayData, ComponentToWorld, Particle);
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

	FBoundingBox BuildSpriteInstanceBounds(
		const TArray<FParticleSpriteInstanceData>& Instances,
		uint32 FirstInstance,
		uint32 InstanceCount)
	{
		FBoundingBox Bounds;
		for (uint32 InstanceIndex = FirstInstance; InstanceIndex < FirstInstance + InstanceCount; ++InstanceIndex)
		{
			const FParticleSpriteInstanceData& Instance = Instances[InstanceIndex];
			Bounds.Expand(Instance.Center + Instance.AxisX + Instance.AxisY);
			Bounds.Expand(Instance.Center + Instance.AxisX - Instance.AxisY);
			Bounds.Expand(Instance.Center - Instance.AxisX + Instance.AxisY);
			Bounds.Expand(Instance.Center - Instance.AxisX - Instance.AxisY);
		}
		return Bounds;
	}
}

FParticleSystemRenderProxy::FParticleSystemRenderProxy(UParticleSystemComponent* InComponent)
	: Component(InComponent)
{
}

FParticleSystemRenderProxy::~FParticleSystemRenderProxy()
{
	ReleaseResources();
}

void FParticleSystemRenderProxy::CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context)
{
	if (Component == nullptr)
	{
		return;
	}

	TArray<FRenderCommand> Commands;
	if (!BuildSpriteCommands(Context, Commands) || SpriteInstances.empty())
	{
		return;
	}

	if (!EnsureSpriteInstanceBuffer(Context.Device, static_cast<uint32>(SpriteInstances.size())))
	{
		return;
	}

	if (!UploadSpriteInstances(Context.DeviceContext))
	{
		return;
	}

	for (FRenderCommand& Command : Commands)
	{
		Command.InstanceBufferView.Buffer = SpriteInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}
}

void FParticleSystemRenderProxy::ReleaseResources()
{
	SpriteInstances.clear();
	SpriteInstanceBuffer.Reset();
	MaxSpriteInstanceCount = 0;
}

bool FParticleSystemRenderProxy::BuildSpriteCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutCommands)
{
	SpriteInstances.clear();
	OutCommands.clear();

	const int32 SnapshotCount = Component->GetEmitterRenderDataSnapshotCount();
	for (int32 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = Component->GetEmitterRenderDataSnapshot(SnapshotIndex);
		if (EmitterData == nullptr)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& ReplayData = EmitterData->GetSource();
		if (EmitterData->GetEmitterType() != EDynamicEmitterType::Sprite || ReplayData.ActiveParticleCount <= 0)
		{
			continue;
		}

		const uint32 FirstInstance = static_cast<uint32>(SpriteInstances.size());
		const TArray<int32> SortedIndices = BuildSortedActiveIndices(
			ReplayData,
			EmitterData->ComponentToWorld,
			Context.RenderBus);

		for (int32 ActiveIndex : SortedIndices)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle != nullptr)
			{
				AppendParticleSpriteInstance(
					ReplayData,
					EmitterData->ComponentToWorld,
					*Particle,
					Context.RenderBus.GetCameraRight(),
					Context.RenderBus.GetCameraUp(),
					SpriteInstances);
			}
		}

		const uint32 InstanceCount = static_cast<uint32>(SpriteInstances.size()) - FirstInstance;
		if (InstanceCount == 0)
		{
			continue;
		}

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Particle;
		Cmd.SourcePrimitive = Component;
		Cmd.Material = EmitterData->Material;
		Cmd.ParticleEmitterData = EmitterData;
		Cmd.VertexFactoryType = EVertexFactoryType::ParticleSprite;
		Cmd.PerObjectConstants = FPerObjectConstants{ FMatrix::Identity, FColor::White().ToVector4() };
		Cmd.WorldAABB = BuildSpriteInstanceBounds(SpriteInstances, FirstInstance, InstanceCount);
		if (!Cmd.WorldAABB.IsValid())
		{
			Cmd.WorldAABB = Component->GetWorldAABB();
		}
		Cmd.Constants.Particle.ComponentToWorld = EmitterData->ComponentToWorld;
		Cmd.Constants.Particle.CameraRight = Context.RenderBus.GetCameraRight();
		Cmd.Constants.Particle.CameraUp = Context.RenderBus.GetCameraUp();
		Cmd.Constants.Particle.EmitterType = static_cast<uint32>(EmitterData->GetEmitterType());
		Cmd.Constants.Particle.CoordinateSpace = static_cast<uint32>(ReplayData.CoordinateSpace);
		Cmd.Constants.Particle.ActiveParticleCount = static_cast<uint32>(ReplayData.ActiveParticleCount);
		Cmd.Constants.Particle.bUseLocalSpace = ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local ? 1u : 0u;
		Cmd.InstanceBufferView.InstanceCount = InstanceCount;
		Cmd.InstanceBufferView.Stride = sizeof(FParticleSpriteInstanceData);
		Cmd.InstanceBufferView.Offset = FirstInstance * sizeof(FParticleSpriteInstanceData);

		OutCommands.push_back(std::move(Cmd));
	}

	return true;
}

bool FParticleSystemRenderProxy::EnsureSpriteInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
{
	if (Device == nullptr)
	{
		return false;
	}

	if (SpriteInstanceBuffer && InstanceCount <= MaxSpriteInstanceCount)
	{
		return true;
	}

	MaxSpriteInstanceCount = std::max(InstanceCount * 2u, 1u);
	SpriteInstanceBuffer.Reset();

	D3D11_BUFFER_DESC InstanceDesc = {};
	InstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	InstanceDesc.ByteWidth = sizeof(FParticleSpriteInstanceData) * MaxSpriteInstanceCount;
	InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(Device->CreateBuffer(&InstanceDesc, nullptr, SpriteInstanceBuffer.ReleaseAndGetAddressOf()));
}

bool FParticleSystemRenderProxy::UploadSpriteInstances(ID3D11DeviceContext* DeviceContext)
{
	if (DeviceContext == nullptr || !SpriteInstanceBuffer)
	{
		return false;
	}

	if (SpriteInstances.empty())
	{
		return true;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(SpriteInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	std::memcpy(
		Mapped.pData,
		SpriteInstances.data(),
		sizeof(FParticleSpriteInstanceData) * SpriteInstances.size());
	DeviceContext->Unmap(SpriteInstanceBuffer.Get(), 0);
	return true;
}
