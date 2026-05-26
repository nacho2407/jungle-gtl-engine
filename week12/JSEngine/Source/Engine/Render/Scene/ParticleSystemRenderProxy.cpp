#include "ParticleSystemRenderProxy.h"

#include "Particle/ParticleSystemComponent.h"
#include "Particle/ParticleMeshBounds.h"
#include "Particle/ParticleModules.h"
#include "Particle/ParticleTypes.h"
#include "Asset/StaticMesh.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Scene/RenderBus.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <unordered_set>
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
		const float RotationRadians = Particle.Rotation * (3.14159265358979323846f / 180.0f);
		const float CosRotation = std::cos(RotationRadians);
		const float SinRotation = std::sin(RotationRadians);
		const FVector RotatedAxisX = CameraRight * CosRotation + CameraUp * SinRotation;
		const FVector RotatedAxisY = CameraUp * CosRotation - CameraRight * SinRotation;

		Instances.push_back({
			WorldLocation,
			RotatedAxisX * HalfW,
			RotatedAxisY * HalfH,
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

	bool BuildParticleSubUVCommands(
		UParticleSystemComponent* Component,
		const FDynamicEmitterDataBase& EmitterData,
		const FDynamicSpriteEmitterReplayDataBase& ReplayData,
		const TArray<int32>& SortedIndices,
		TArray<FRenderCommand>& OutCommands)
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
			return true;
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
			FMatrix Model = FMatrix::MakeScale(Particle->Size);
			Model.SetOrigin(GetParticleWorldLocation(ReplayData, EmitterData.ComponentToWorld, *Particle));

			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::SubUV;
			Cmd.SourcePrimitive = Component;
			Cmd.Material = EmitterData.Material;
			Cmd.ParticleEmitterData = &EmitterData;
			Cmd.ParticleReplayData = &ReplayData;
			Cmd.VertexFactoryType = EVertexFactoryType::SubUV;
			Cmd.PerObjectConstants = FPerObjectConstants{ Model, Particle->Color.ToVector4() };
			Cmd.WorldAABB = Component ? Component->GetWorldAABB() : FBoundingBox{};
			Cmd.Constants.SubUV.Texture = ReplayData.SubUVTexture;
			Cmd.Constants.SubUV.FrameIndex = FrameIndex;
			Cmd.Constants.SubUV.Columns = static_cast<uint32>(Columns);
			Cmd.Constants.SubUV.Rows = static_cast<uint32>(Rows);
			Cmd.Constants.SubUV.Width = 1.0f;
			Cmd.Constants.SubUV.Height = 1.0f;
			Cmd.Constants.SubUV.Color = Particle->Color;

			OutCommands.push_back(std::move(Cmd));
		}

		return true;
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

	UMaterialInterface* ResolveMeshParticleMaterial(UMaterialInterface* Material)
	{
		return Material != nullptr ? Material : FResourceManager::Get().GetMaterial("DefaultWhite");
	}

	/**
	 * @brief Beam particle command가 사용할 material을 선택합니다.
	 */
	UMaterialInterface* ResolveBeamParticleMaterial(UMaterialInterface* Material)
	{
		return Material != nullptr ? Material : FResourceManager::Get().GetMaterial("DefaultWhite");
	}

	/**
	 * @brief Beam replay point를 world space point로 변환합니다.
	 */
	FVector GetBeamWorldPoint(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FVector& Point)
	{
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformPosition(Point)
			: Point;
	}

	/**
	 * @brief Beam source/target과 첫 live particle의 width scale에서 보수적인 world bounds를 생성합니다.
	 */
	FBoundingBox BuildBeamWorldBounds(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FBaseParticle& Particle)
	{
		// source / target 좌표계 반영
		const FVector Source = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.SourcePoint);
		const FVector Target = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.TargetPoint);

		// BeamWidth와 Particle.Size.X를 함께 고려한 보수적 두께
		const float ParticleWidthScale = std::max(std::fabs(Particle.Size.X), 0.001f);
		const float HalfWidth = std::max(ReplayData.BeamWidth * ParticleWidthScale, 0.1f) * 0.5f;
		const FVector Extent(HalfWidth, HalfWidth, HalfWidth);

		// source / target 양 끝점을 모두 포함하는 AABB
		FBoundingBox Bounds;
		Bounds.Expand(Source - Extent);
		Bounds.Expand(Source + Extent);
		Bounds.Expand(Target - Extent);
		Bounds.Expand(Target + Extent);
		return Bounds;
	}

	enum class EParticleProxyDiagnostic : uint32
	{
		EmptyActiveParticles = 1,
		MissingMesh,
		MissingMeshBuffer,
		MissingSectionMaterial,
		UnsupportedEmitterType,
	};

	void LogParticleDiagnosticOnce(
		const void* Component,
		int32 EmitterIndex,
		EParticleProxyDiagnostic Diagnostic,
		const char* Message)
	{
		static std::unordered_set<uint64> LoggedDiagnostics;
		const uint64 ComponentKey = static_cast<uint64>(reinterpret_cast<std::uintptr_t>(Component) >> 4);
		const uint64 Key = ComponentKey
			^ (static_cast<uint64>(static_cast<uint32>(EmitterIndex)) << 32)
			^ static_cast<uint64>(Diagnostic);
		if (LoggedDiagnostics.insert(Key).second)
		{
			UE_LOG_WARNING("%s", Message);
		}
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

	TArray<FRenderCommand> SpriteCommands;
	TArray<FRenderCommand> SubUVCommands;
	TArray<FRenderCommand> OpaqueMeshCommands;
	TArray<FRenderCommand> TranslucentMeshCommands;
	TArray<FRenderCommand> OpaqueBeamCommands;
	TArray<FRenderCommand> TranslucentBeamCommands;
	BuildSpriteCommands(Context, SpriteCommands, SubUVCommands);
	BuildMeshCommands(Context, OpaqueMeshCommands, TranslucentMeshCommands);
	BuildBeamCommands(Context, OpaqueBeamCommands, TranslucentBeamCommands);

	if (SpriteInstances.empty() &&
		MeshInstances.empty() &&
		SubUVCommands.empty() &&
		OpaqueBeamCommands.empty() &&
		TranslucentBeamCommands.empty())
	{
		return;
	}

	if (!SpriteInstances.empty() && !EnsureSpriteInstanceBuffer(Context.Device, static_cast<uint32>(SpriteInstances.size())))
	{
		return;
	}

	if (!MeshInstances.empty() && !EnsureMeshInstanceBuffer(Context.Device, static_cast<uint32>(MeshInstances.size())))
	{
		return;
	}

	if (!SpriteInstances.empty() && !UploadSpriteInstances(Context.DeviceContext))
	{
		return;
	}

	if (!MeshInstances.empty() && !UploadMeshInstances(Context.DeviceContext))
	{
		return;
	}

	for (FRenderCommand& Command : SpriteCommands)
	{
		Command.InstanceBufferView.Buffer = SpriteInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}

	for (FRenderCommand& Command : SubUVCommands)
	{
		Context.RenderBus.AddCommand(ERenderPass::SubUV, std::move(Command));
	}

	for (FRenderCommand& Command : OpaqueMeshCommands)
	{
		Command.InstanceBufferView.Buffer = MeshInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Opaque, std::move(Command));
	}

	for (FRenderCommand& Command : OpaqueBeamCommands)
	{
		Context.RenderBus.AddCommand(ERenderPass::Opaque, std::move(Command));
	}

	for (FRenderCommand& Command : TranslucentMeshCommands)
	{
		Command.InstanceBufferView.Buffer = MeshInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}

	for (FRenderCommand& Command : TranslucentBeamCommands)
	{
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}
}

void FParticleSystemRenderProxy::ReleaseResources()
{
	SpriteInstances.clear();
	MeshInstances.clear();
	SpriteInstanceBuffer.Reset();
	MeshInstanceBuffer.Reset();
	MaxSpriteInstanceCount = 0;
	MaxMeshInstanceCount = 0;
}

bool FParticleSystemRenderProxy::BuildSpriteCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutSpriteCommands,
	TArray<FRenderCommand>& OutSubUVCommands)
{
	SpriteInstances.clear();
	OutSpriteCommands.clear();
	OutSubUVCommands.clear();

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
		const FDynamicSpriteEmitterReplayDataBase* SpriteReplayData =
			static_cast<const FDynamicSpriteEmitterReplayDataBase*>(&ReplayData);
		if (BuildParticleSubUVCommands(Component, *EmitterData, *SpriteReplayData, SortedIndices, OutSubUVCommands))
		{
			continue;
		}

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

		OutSpriteCommands.push_back(std::move(Cmd));
	}

	return true;
}

bool FParticleSystemRenderProxy::BuildMeshCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutOpaqueCommands,
	TArray<FRenderCommand>& OutTranslucentCommands)
{
	MeshInstances.clear();
	OutOpaqueCommands.clear();
	OutTranslucentCommands.clear();

	const int32 SnapshotCount = Component->GetEmitterRenderDataSnapshotCount();
	for (int32 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = Component->GetEmitterRenderDataSnapshot(SnapshotIndex);
		if (EmitterData == nullptr)
		{
			continue;
		}

		if (EmitterData->GetEmitterType() != EDynamicEmitterType::Mesh)
		{
			if (EmitterData->GetEmitterType() != EDynamicEmitterType::Sprite &&
				EmitterData->GetEmitterType() != EDynamicEmitterType::Beam)
			{
				LogParticleDiagnosticOnce(
					Component,
					EmitterData->EmitterIndex,
					EParticleProxyDiagnostic::UnsupportedEmitterType,
					"[Particle] Unsupported emitter type skipped by particle render proxy.");
			}
			continue;
		}

		const FDynamicMeshEmitterData* MeshEmitterData = static_cast<const FDynamicMeshEmitterData*>(EmitterData);
		const FDynamicEmitterReplayDataBase& ReplayData = MeshEmitterData->GetSource();
		if (ReplayData.ActiveParticleCount <= 0)
		{
			LogParticleDiagnosticOnce(
				Component,
				EmitterData->EmitterIndex,
				EParticleProxyDiagnostic::EmptyActiveParticles,
				"[Particle] Mesh emitter has no active particles.");
			continue;
		}

		const UStaticMesh* Mesh = MeshEmitterData->Mesh;
		if (Mesh == nullptr || !Mesh->HasValidMeshData())
		{
			LogParticleDiagnosticOnce(
				Component,
				EmitterData->EmitterIndex,
				EParticleProxyDiagnostic::MissingMesh,
				"[Particle] Mesh emitter skipped because its static mesh is missing.");
			continue;
		}

		FMeshBuffer* MeshBuffer = Context.MeshBufferManager.GetStaticMeshBuffer(Mesh, 0);
		if (MeshBuffer == nullptr)
		{
			LogParticleDiagnosticOnce(
				Component,
				EmitterData->EmitterIndex,
				EParticleProxyDiagnostic::MissingMeshBuffer,
				"[Particle] Mesh emitter skipped because LOD 0 mesh buffer is missing.");
			continue;
		}

		const FStaticMesh* MeshData = Mesh->GetMeshData(0);
		if (MeshData == nullptr || MeshData->Sections.empty())
		{
			continue;
		}

		const uint32 FirstInstance = static_cast<uint32>(MeshInstances.size());
		const TArray<int32> SortedIndices = BuildSortedActiveIndices(
			ReplayData,
			EmitterData->ComponentToWorld,
			Context.RenderBus);

		for (int32 ActiveIndex : SortedIndices)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle == nullptr)
			{
				continue;
			}

			MeshInstances.push_back({
				ParticleMeshBounds::BuildInstanceTransform(ReplayData, EmitterData->ComponentToWorld, *Particle)
			});
		}

		const uint32 InstanceCount = static_cast<uint32>(MeshInstances.size()) - FirstInstance;
		if (InstanceCount == 0)
		{
			continue;
		}

		const FBoundingBox MeshParticleBounds = ParticleMeshBounds::BuildConservativeWorldBounds(
			ReplayData,
			EmitterData->ComponentToWorld,
			MeshData->LocalBounds);

		for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(MeshData->Sections.size()); ++SectionIdx)
		{
			const FStaticMeshSection& Section = MeshData->Sections[SectionIdx];
			if (Section.IndexCount == 0)
			{
				continue;
			}

			UMaterialInterface* SectionMaterial = nullptr;
			if (Section.MaterialSlotIndex >= 0 &&
				Section.MaterialSlotIndex < static_cast<int32>(MeshData->Slots.size()))
			{
				SectionMaterial = MeshData->Slots[Section.MaterialSlotIndex].Material;
			}

			if (SectionMaterial == nullptr)
			{
				LogParticleDiagnosticOnce(
					Component,
					EmitterData->EmitterIndex,
					EParticleProxyDiagnostic::MissingSectionMaterial,
					"[Particle] Mesh emitter section material missing. Falling back to DefaultWhite.");
			}

			FRenderCommand Cmd = {};
			Cmd.Type = ERenderCommandType::Particle;
			Cmd.SourcePrimitive = Component;
			Cmd.Material = ResolveMeshParticleMaterial(SectionMaterial);
			Cmd.ParticleEmitterData = EmitterData;
			Cmd.ParticleReplayData = &ReplayData;
			Cmd.VertexFactoryType = EVertexFactoryType::ParticleMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.PerObjectConstants = FPerObjectConstants{ FMatrix::Identity, FColor::White().ToVector4() };
			Cmd.WorldAABB = MeshParticleBounds.IsValid() ? MeshParticleBounds : Component->GetWorldAABB();
			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;
			Cmd.InstanceBufferView.InstanceCount = InstanceCount;
			Cmd.InstanceBufferView.Stride = sizeof(FParticleMeshInstanceData);
			Cmd.InstanceBufferView.Offset = FirstInstance * sizeof(FParticleMeshInstanceData);

			if (ResolveMaterialRenderPass(Cmd.Material) == ERenderPass::Translucent)
			{
				OutTranslucentCommands.push_back(std::move(Cmd));
			}
			else
			{
				OutOpaqueCommands.push_back(std::move(Cmd));
			}
		}
	}

	return true;
}

bool FParticleSystemRenderProxy::BuildBeamCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutOpaqueCommands,
	TArray<FRenderCommand>& OutTranslucentCommands)
{
	OutOpaqueCommands.clear();
	OutTranslucentCommands.clear();

	// ParticleSystemComponent가 만들어둔 emitter render snapshot 순회
	const int32 SnapshotCount = Component->GetEmitterRenderDataSnapshotCount();
	for (int32 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = Component->GetEmitterRenderDataSnapshot(SnapshotIndex);
		if (EmitterData == nullptr || EmitterData->GetEmitterType() != EDynamicEmitterType::Beam)
		{
			continue;
		}

		// Beam replay data와 active particle 유효성
		const FDynamicBeamEmitterData* BeamEmitterData = static_cast<const FDynamicBeamEmitterData*>(EmitterData);
		const FDynamicBeamEmitterReplayDataBase& ReplayData = BeamEmitterData->ReplayData;
		if (ReplayData.ActiveParticleCount <= 0)
		{
			continue;
		}

		// 최소 Beam은 첫 번째 live particle의 색상과 Size.X를 render 입력으로 사용
		const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(0);
		if (Particle == nullptr)
		{
			continue;
		}

		// material 누락 시에도 command 경로를 확인할 수 있는 fallback material
		UMaterialInterface* BeamMaterial = ResolveBeamParticleMaterial(EmitterData->Material);

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Particle;
		Cmd.SourcePrimitive = Component;
		Cmd.Material = BeamMaterial;
		Cmd.ParticleEmitterData = EmitterData;
		Cmd.ParticleReplayData = &ReplayData;
		Cmd.PerObjectConstants = FPerObjectConstants{ FMatrix::Identity, FColor::White().ToVector4() };

		// source / target / width 기준 command bounds. 실패 시 component bounds fallback
		Cmd.WorldAABB = BuildBeamWorldBounds(ReplayData, EmitterData->ComponentToWorld, *Particle);
		if (!Cmd.WorldAABB.IsValid())
		{
			Cmd.WorldAABB = Component->GetWorldAABB();
		}

		// Beam draw path가 사용할 공통 particle constants
		Cmd.Constants.Particle.ComponentToWorld = EmitterData->ComponentToWorld;
		Cmd.Constants.Particle.CameraRight = Context.RenderBus.GetCameraRight();
		Cmd.Constants.Particle.CameraUp = Context.RenderBus.GetCameraUp();
		Cmd.Constants.Particle.EmitterType = static_cast<uint32>(EmitterData->GetEmitterType());
		Cmd.Constants.Particle.CoordinateSpace = static_cast<uint32>(ReplayData.CoordinateSpace);
		Cmd.Constants.Particle.ActiveParticleCount = static_cast<uint32>(ReplayData.ActiveParticleCount);
		Cmd.Constants.Particle.bUseLocalSpace = ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local ? 1u : 0u;

		// material 정책에 따른 pass queue 분배
		if (ResolveMaterialRenderPass(Cmd.Material) == ERenderPass::Translucent)
		{
			OutTranslucentCommands.push_back(std::move(Cmd));
		}
		else
		{
			OutOpaqueCommands.push_back(std::move(Cmd));
		}
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

bool FParticleSystemRenderProxy::EnsureMeshInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
{
	if (Device == nullptr)
	{
		return false;
	}

	if (MeshInstanceBuffer && InstanceCount <= MaxMeshInstanceCount)
	{
		return true;
	}

	MaxMeshInstanceCount = std::max(InstanceCount * 2u, 1u);
	MeshInstanceBuffer.Reset();

	D3D11_BUFFER_DESC InstanceDesc = {};
	InstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	InstanceDesc.ByteWidth = sizeof(FParticleMeshInstanceData) * MaxMeshInstanceCount;
	InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(Device->CreateBuffer(&InstanceDesc, nullptr, MeshInstanceBuffer.ReleaseAndGetAddressOf()));
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

bool FParticleSystemRenderProxy::UploadMeshInstances(ID3D11DeviceContext* DeviceContext)
{
	if (DeviceContext == nullptr || !MeshInstanceBuffer)
	{
		return false;
	}

	if (MeshInstances.empty())
	{
		return true;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(MeshInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	std::memcpy(
		Mapped.pData,
		MeshInstances.data(),
		sizeof(FParticleMeshInstanceData) * MeshInstances.size());
	DeviceContext->Unmap(MeshInstanceBuffer.Get(), 0);
	return true;
}
