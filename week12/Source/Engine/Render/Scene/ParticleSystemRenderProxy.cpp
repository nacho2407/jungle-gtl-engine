#include "ParticleSystemRenderProxy.h"

#include "Particle/ParticleSystemComponent.h"
#include "Particle/ParticleBeamPath.h"
#include "Particle/ParticleMeshBounds.h"
#include "Particle/ParticleModules.h"
#include "Particle/ParticleTypes.h"
#include "Core/Logging/Stats.h"
#include "Asset/StaticMesh.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/Material.h"
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

		SCOPE_STAT("Particle.EmitterSort");
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
		const FVector4& UVRect,
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
			Particle.Color,
			UVRect
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

	FVector4 BuildParticleSpriteUVRect(
		const FDynamicSpriteEmitterReplayDataBase& ReplayData,
		const FBaseParticle& Particle,
		bool& bOutHasValidSubUVPayload)
	{
		bOutHasValidSubUVPayload = false;
		if (ReplayData.SubUVPayloadOffset < 0 || ReplayData.SubUVTexture == nullptr)
		{
			return FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		const int32 Columns = std::max(ReplayData.SubUVColumns, 1);
		const int32 Rows = std::max(ReplayData.SubUVRows, 1);
		const int32 TotalFrames = Columns * Rows;
		if (TotalFrames <= 0)
		{
			return FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		const FSubUVParticlePayload* Payload = GetSubUVPayloadFromSnapshot(ReplayData, Particle);
		if (Payload == nullptr)
		{
			return FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		const uint32 FrameIndex = static_cast<uint32>(std::clamp(
			static_cast<int32>(Payload->ImageIndex + 0.5f),
			0,
			TotalFrames - 1));
		const float FrameW = 1.0f / static_cast<float>(Columns);
		const float FrameH = 1.0f / static_cast<float>(Rows);
		const uint32 Col = FrameIndex % static_cast<uint32>(Columns);
		const uint32 Row = FrameIndex / static_cast<uint32>(Columns);
		bOutHasValidSubUVPayload = true;
		return FVector4(
			static_cast<float>(Col) * FrameW,
			static_cast<float>(Row) * FrameH,
			FrameW,
			FrameH);
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

	ERenderPass ResolveBeamParticleRenderPass(const UMaterialInterface* Material)
	{
		if (Material == nullptr)
		{
			return ERenderPass::Opaque;
		}

		return Material->GetBlendMode() == EMaterialBlendMode::Translucent ||
			Material->GetBlendStateDesc().bBlendEnable
			? ERenderPass::Translucent
			: ResolveMaterialRenderPass(Material);
	}

	UMaterialInterface* ResolveRibbonParticleMaterial(UMaterialInterface* Material)
	{
		return Material != nullptr ? Material : FResourceManager::Get().GetMaterial("DefaultWhite");
	}

	ERenderPass ResolveRibbonParticleRenderPass(const UMaterialInterface* Material)
	{
		if (Material == nullptr)
		{
			return ERenderPass::Translucent;
		}

		return Material->GetBlendMode() == EMaterialBlendMode::Translucent ||
			Material->GetBlendStateDesc().bBlendEnable
			? ERenderPass::Translucent
			: ResolveMaterialRenderPass(Material);
	}

	FVector GetRibbonWorldPoint(
		const FDynamicRibbonEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FRibbonRenderPoint& Point)
	{
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformPosition(Point.Position)
			: Point.Position;
	}

	FBoundingBox BuildRibbonWorldBounds(
		const FDynamicRibbonEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld)
	{
		FBoundingBox Bounds;
		for (const FRibbonRenderPoint& Point : ReplayData.RenderPoints)
		{
			const FVector WorldPoint = GetRibbonWorldPoint(ReplayData, ComponentToWorld, Point);
			const float HalfWidth = std::max(Point.Width, 0.1f) * 0.5f;
			const FVector Extent(HalfWidth, HalfWidth, HalfWidth);
			Bounds.Expand(WorldPoint - Extent);
			Bounds.Expand(WorldPoint + Extent);
		}
		return Bounds;
	}

	FVector GetRibbonSourceSide(const FRibbonRenderPoint& Point)
	{
		FVector Side = Point.Side.GetSafeNormal();
		return Side.IsNearlyZero() ? FVector::RightVector : Side;
	}

	FVector GetRibbonPointTangent(
		const FDynamicRibbonEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FRibbonRenderRange& Range,
		int32 PointIndex)
	{
		const int32 RangeEnd = Range.PointStart + Range.PointCount;
		const FVector Current = GetRibbonWorldPoint(
			ReplayData,
			ComponentToWorld,
			ReplayData.RenderPoints[static_cast<size_t>(PointIndex)]);

		FVector Tangent = FVector::ZeroVector;
		if (PointIndex <= Range.PointStart && PointIndex + 1 < RangeEnd)
		{
			const FVector Next = GetRibbonWorldPoint(
				ReplayData,
				ComponentToWorld,
				ReplayData.RenderPoints[static_cast<size_t>(PointIndex + 1)]);
			Tangent = Next - Current;
		}
		else if (PointIndex >= RangeEnd - 1 && PointIndex - 1 >= Range.PointStart)
		{
			const FVector Prev = GetRibbonWorldPoint(
				ReplayData,
				ComponentToWorld,
				ReplayData.RenderPoints[static_cast<size_t>(PointIndex - 1)]);
			Tangent = Current - Prev;
		}
		else if (PointIndex - 1 >= Range.PointStart && PointIndex + 1 < RangeEnd)
		{
			const FVector Prev = GetRibbonWorldPoint(
				ReplayData,
				ComponentToWorld,
				ReplayData.RenderPoints[static_cast<size_t>(PointIndex - 1)]);
			const FVector Next = GetRibbonWorldPoint(
				ReplayData,
				ComponentToWorld,
				ReplayData.RenderPoints[static_cast<size_t>(PointIndex + 1)]);
			Tangent = Next - Prev;
		}

		Tangent = Tangent.GetSafeNormal();
		return Tangent.IsNearlyZero() ? FVector::ForwardVector : Tangent;
	}

	FVector ResolveRibbonPointSide(
		const FDynamicRibbonEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FRibbonRenderRange& Range,
		int32 PointIndex,
		const FVector& CameraPosition,
		const FVector& CameraRight)
	{
		const FRibbonRenderPoint& Point = ReplayData.RenderPoints[static_cast<size_t>(PointIndex)];
		if (ReplayData.RibbonFacingMode == EParticleRibbonFacingMode::SourceTransform)
		{
			return GetRibbonSourceSide(Point);
		}

		const FVector WorldPoint = GetRibbonWorldPoint(ReplayData, ComponentToWorld, Point);
		FVector ToCamera = (CameraPosition - WorldPoint).GetSafeNormal();
		if (ToCamera.IsNearlyZero())
		{
			ToCamera = FVector::ForwardVector;
		}

		const FVector Tangent = GetRibbonPointTangent(ReplayData, ComponentToWorld, Range, PointIndex);
		FVector Side = FVector::CrossProduct(ToCamera, Tangent).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			Side = CameraRight.GetSafeNormal();
		}
		return Side.IsNearlyZero() ? FVector::RightVector : Side;
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

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: Component(InComponent)
{
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	ReleaseResources();
}

void FParticleSystemSceneProxy::CollectCommands(const FPrimitiveRenderProxyCollectionContext& Context)
{
	if (Component == nullptr || !Context.ShowFlags.bParticle)
	{
		return;
	}

	TArray<FRenderCommand> SpriteCommands;
	TArray<FRenderCommand> OpaqueMeshCommands;
	TArray<FRenderCommand> TranslucentMeshCommands;
	TArray<FRenderCommand> OpaqueBeamCommands;
	TArray<FRenderCommand> TranslucentBeamCommands;
	TArray<FRenderCommand> OpaqueRibbonCommands;
	TArray<FRenderCommand> TranslucentRibbonCommands;
	FParticleFrameStats ParticleStats = Component->GetLastParticleFrameStats();

	BuildSpriteCommands(Context, SpriteCommands);
	BuildMeshCommands(Context, OpaqueMeshCommands, TranslucentMeshCommands);
	BuildBeamCommands(Context, OpaqueBeamCommands, TranslucentBeamCommands);
	BuildRibbonCommands(Context, OpaqueRibbonCommands, TranslucentRibbonCommands);

	if (SpriteInstances.empty() &&
		MeshInstances.empty() &&
		BeamInstances.empty() &&
		OpaqueBeamCommands.empty() &&
		TranslucentBeamCommands.empty() &&
		OpaqueRibbonCommands.empty() &&
		TranslucentRibbonCommands.empty())
	{
		Context.CommandServices.ParticleStats.Accumulate(ParticleStats);
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

	if (!BeamInstances.empty() && !EnsureBeamInstanceBuffer(Context.Device, static_cast<uint32>(BeamInstances.size())))
	{
		return;
	}

	if (!RibbonInstances.empty() && !EnsureRibbonInstanceBuffer(Context.Device, static_cast<uint32>(RibbonInstances.size())))
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

	if (!BeamInstances.empty() && !UploadBeamInstances(Context.DeviceContext))
	{
		return;
	}

	if (!RibbonInstances.empty() && !UploadRibbonInstances(Context.DeviceContext))
	{
		return;
	}

	for (const FRenderCommand& Command : SpriteCommands)
	{
		ParticleStats.SpriteParticleCount += static_cast<int32>(Command.InstanceBufferView.InstanceCount);
		++ParticleStats.ParticleDrawCalls;
	}

	std::unordered_set<const FDynamicEmitterReplayDataBase*> CountedMeshReplayData;
	auto AccumulateMeshCommandStats = [&ParticleStats, &CountedMeshReplayData](const FRenderCommand& Command)
	{
		if (Command.ParticleReplayData != nullptr &&
			CountedMeshReplayData.insert(Command.ParticleReplayData).second)
		{
			ParticleStats.MeshParticleCount += Command.ParticleReplayData->ActiveParticleCount;
		}
		ParticleStats.MeshParticlePolygons +=
			(static_cast<uint64>(Command.SectionIndexCount) / 3ull) *
			static_cast<uint64>(Command.InstanceBufferView.InstanceCount);
		++ParticleStats.ParticleDrawCalls;
	};

	for (const FRenderCommand& Command : OpaqueMeshCommands)
	{
		AccumulateMeshCommandStats(Command);
	}

	for (const FRenderCommand& Command : TranslucentMeshCommands)
	{
		AccumulateMeshCommandStats(Command);
	}

	auto AccumulateBeamCommandStats = [&ParticleStats](const FRenderCommand& Command)
	{
		const uint32 ActiveParticleCount = Command.ParticleReplayData != nullptr
			? static_cast<uint32>(Command.ParticleReplayData->ActiveParticleCount)
			: Command.Constants.Particle.ActiveParticleCount;
		ParticleStats.BeamParticleCount += static_cast<int32>(ActiveParticleCount);
		ParticleStats.BeamParticlePolygons +=
			static_cast<uint64>(Command.InstanceBufferView.InstanceCount) * 2ull;
		++ParticleStats.ParticleDrawCalls;
	};

	for (const FRenderCommand& Command : OpaqueBeamCommands)
	{
		AccumulateBeamCommandStats(Command);
	}

	for (const FRenderCommand& Command : TranslucentBeamCommands)
	{
		AccumulateBeamCommandStats(Command);
	}

	std::unordered_set<const FDynamicEmitterReplayDataBase*> CountedRibbonReplayData;
	auto AccumulateRibbonCommandStats = [&ParticleStats, &CountedRibbonReplayData](const FRenderCommand& Command)
	{
		if (const FDynamicRibbonEmitterReplayDataBase* ReplayData =
			static_cast<const FDynamicRibbonEmitterReplayDataBase*>(Command.ParticleReplayData))
		{
			if (CountedRibbonReplayData.insert(ReplayData).second)
			{
				ParticleStats.TrailParticleCount += static_cast<int32>(ReplayData->RenderPoints.size());
			}
		}
		ParticleStats.TrailParticlePolygons +=
			static_cast<uint64>(Command.InstanceBufferView.InstanceCount) * 2ull;
		++ParticleStats.ParticleDrawCalls;
	};

	for (const FRenderCommand& Command : OpaqueRibbonCommands)
	{
		AccumulateRibbonCommandStats(Command);
	}

	for (const FRenderCommand& Command : TranslucentRibbonCommands)
	{
		AccumulateRibbonCommandStats(Command);
	}

	Context.CommandServices.ParticleStats.Accumulate(ParticleStats);

	for (FRenderCommand& Command : SpriteCommands)
	{
		Command.InstanceBufferView.Buffer = SpriteInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}

	for (FRenderCommand& Command : OpaqueMeshCommands)
	{
		Command.InstanceBufferView.Buffer = MeshInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Opaque, std::move(Command));
	}

	for (FRenderCommand& Command : OpaqueBeamCommands)
	{
		Command.InstanceBufferView.Buffer = BeamInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Opaque, std::move(Command));
	}

	for (FRenderCommand& Command : OpaqueRibbonCommands)
	{
		Command.InstanceBufferView.Buffer = RibbonInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Opaque, std::move(Command));
	}

	for (FRenderCommand& Command : TranslucentMeshCommands)
	{
		Command.InstanceBufferView.Buffer = MeshInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}

	for (FRenderCommand& Command : TranslucentBeamCommands)
	{
		Command.InstanceBufferView.Buffer = BeamInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}

	for (FRenderCommand& Command : TranslucentRibbonCommands)
	{
		Command.InstanceBufferView.Buffer = RibbonInstanceBuffer.Get();
		Context.RenderBus.AddCommand(ERenderPass::Translucent, std::move(Command));
	}
}

void FParticleSystemSceneProxy::ReleaseResources()
{
	SpriteInstances.clear();
	MeshInstances.clear();
	BeamInstances.clear();
	RibbonInstances.clear();
	SpriteInstanceBuffer.Reset();
	MeshInstanceBuffer.Reset();
	BeamInstanceBuffer.Reset();
	RibbonInstanceBuffer.Reset();
	MaxSpriteInstanceCount = 0;
	MaxMeshInstanceCount = 0;
	MaxBeamInstanceCount = 0;
	MaxRibbonInstanceCount = 0;
}

bool FParticleSystemSceneProxy::BuildSpriteCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutSpriteCommands)
{
	SpriteInstances.clear();
	OutSpriteCommands.clear();

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
		const bool bUseSubUVTexture = SpriteReplayData->SubUVPayloadOffset >= 0 && SpriteReplayData->SubUVTexture != nullptr;

		for (int32 ActiveIndex : SortedIndices)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle != nullptr)
			{
				bool bHasValidSubUVPayload = false;
				const FVector4 UVRect = BuildParticleSpriteUVRect(*SpriteReplayData, *Particle, bHasValidSubUVPayload);
				if (bUseSubUVTexture && !bHasValidSubUVPayload)
				{
					continue;
				}

				AppendParticleSpriteInstance(
					ReplayData,
					EmitterData->ComponentToWorld,
					*Particle,
					Context.RenderBus.GetCameraRight(),
					Context.RenderBus.GetCameraUp(),
					UVRect,
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
		Cmd.Constants.Particle.Texture = bUseSubUVTexture ? SpriteReplayData->SubUVTexture : nullptr;
		Cmd.InstanceBufferView.InstanceCount = InstanceCount;
		Cmd.InstanceBufferView.Stride = sizeof(FParticleSpriteInstanceData);
		Cmd.InstanceBufferView.Offset = FirstInstance * sizeof(FParticleSpriteInstanceData);

		OutSpriteCommands.push_back(std::move(Cmd));
	}

	return true;
}

bool FParticleSystemSceneProxy::BuildMeshCommands(
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
				EmitterData->GetEmitterType() != EDynamicEmitterType::Beam &&
				EmitterData->GetEmitterType() != EDynamicEmitterType::Ribbon)
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

		FMeshBuffer* MeshBuffer = Context.ResourceProvider.GetStaticMeshBuffer(Mesh, 0);
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
			Cmd.VertexFactoryType = EVertexFactoryType::InstancedSurface;
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

bool FParticleSystemSceneProxy::BuildBeamCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutOpaqueCommands,
	TArray<FRenderCommand>& OutTranslucentCommands)
{
	BeamInstances.clear();
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
			LogParticleDiagnosticOnce(
				Component,
				EmitterData->EmitterIndex,
				EParticleProxyDiagnostic::EmptyActiveParticles,
				"[Particle] Beam emitter has no active particles.");
			continue;
		}

		// material 누락 시에도 command 경로를 확인할 수 있는 fallback material
		UMaterialInterface* BeamMaterial = ResolveBeamParticleMaterial(EmitterData->Material);
		const uint32 FirstInstance = static_cast<uint32>(BeamInstances.size());

		// replay source / target / tangent / noise를 최종 world space segment point로 전개
		TArray<FVector> SharedBeamPoints;
		ParticleBeamPath::BuildBeamPathPoints(ReplayData, EmitterData->ComponentToWorld, SharedBeamPoints);
		if (SharedBeamPoints.size() < 2)
		{
			continue;
		}

		float MaxHalfWidth = 0.05f;
		FBoundingBox BeamCommandBounds;
		for (int32 ActiveIndex = 0; ActiveIndex < ReplayData.ActiveParticleCount; ++ActiveIndex)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle == nullptr)
			{
				continue;
			}

			const float ParticleWidthScale = std::max(std::fabs(Particle->Size.X), 0.001f);
			const float HalfWidth = std::max(ReplayData.BeamWidth * ParticleWidthScale, 0.0f) * 0.5f;
			MaxHalfWidth = std::max(MaxHalfWidth, HalfWidth);

			const TArray<FVector>* BeamPoints = &SharedBeamPoints;
			TArray<FVector> ParticleBeamPoints;
			if (ActiveIndex > 0 &&
				ReplayData.bNoiseEnabled &&
				ReplayData.NoiseRange > 0.0f &&
				ReplayData.NoiseFrequency > 0)
			{
				FDynamicBeamEmitterReplayDataBase ParticleReplayData = ReplayData;
				const uint32 SeedInput =
					static_cast<uint32>(ReplayData.NoiseSeed) ^
					(static_cast<uint32>(ActiveIndex) * 0x9e3779b9u);
				ParticleReplayData.NoiseSeed = static_cast<int32>(ParticleBeamPath::HashBeamNoise(SeedInput));
				ParticleBeamPath::BuildBeamPathPoints(ParticleReplayData, EmitterData->ComponentToWorld, ParticleBeamPoints);
				if (ParticleBeamPoints.size() >= 2)
				{
					BeamPoints = &ParticleBeamPoints;
				}
			}

			BeamCommandBounds.Merge(ParticleBeamPath::BuildBeamPointWorldBounds(*BeamPoints, HalfWidth));

			for (int32 PointIndex = 0; PointIndex + 1 < static_cast<int32>(BeamPoints->size()); ++PointIndex)
			{
				// Source/Target path는 emitter 단위로 공유하고, particle별 width/color만 instance에 반영합니다.
				BeamInstances.push_back({
					(*BeamPoints)[static_cast<size_t>(PointIndex)],
					(*BeamPoints)[static_cast<size_t>(PointIndex + 1)],
					HalfWidth,
					Particle->Color
				});
			}
		}

		// 유효 segment가 없으면 command 생성 생략
		const uint32 InstanceCount = static_cast<uint32>(BeamInstances.size()) - FirstInstance;
		if (InstanceCount == 0)
		{
			continue;
		}

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Particle;
		Cmd.SourcePrimitive = Component;
		Cmd.Material = BeamMaterial;
		Cmd.ParticleEmitterData = EmitterData;
		Cmd.ParticleReplayData = &ReplayData;
		Cmd.VertexFactoryType = EVertexFactoryType::ParticleBeam;
		Cmd.PerObjectConstants = FPerObjectConstants{ FMatrix::Identity, FColor::White().ToVector4() };

		// source / target / width 기준 command bounds. 실패 시 component bounds fallback
		Cmd.WorldAABB = BeamCommandBounds;
		if (!Cmd.WorldAABB.IsValid())
		{
			Cmd.WorldAABB = ParticleBeamPath::BuildBeamWorldBounds(
				ReplayData,
				EmitterData->ComponentToWorld,
				MaxHalfWidth);
		}
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
		Cmd.InstanceBufferView.InstanceCount = InstanceCount;
		Cmd.InstanceBufferView.Stride = sizeof(FBeamParticleInstanceData);
		Cmd.InstanceBufferView.Offset = FirstInstance * sizeof(FBeamParticleInstanceData);

		// material 정책에 따른 pass queue 분배
		if (ResolveBeamParticleRenderPass(Cmd.Material) == ERenderPass::Translucent)
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

bool FParticleSystemSceneProxy::BuildRibbonCommands(
	const FPrimitiveRenderProxyCollectionContext& Context,
	TArray<FRenderCommand>& OutOpaqueCommands,
	TArray<FRenderCommand>& OutTranslucentCommands)
{
	RibbonInstances.clear();
	OutOpaqueCommands.clear();
	OutTranslucentCommands.clear();

	const int32 SnapshotCount = Component->GetEmitterRenderDataSnapshotCount();
	for (int32 SnapshotIndex = 0; SnapshotIndex < SnapshotCount; ++SnapshotIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = Component->GetEmitterRenderDataSnapshot(SnapshotIndex);
		if (EmitterData == nullptr || EmitterData->GetEmitterType() != EDynamicEmitterType::Ribbon)
		{
			continue;
		}

		const FDynamicRibbonEmitterData* RibbonEmitterData = static_cast<const FDynamicRibbonEmitterData*>(EmitterData);
		const FDynamicRibbonEmitterReplayDataBase& ReplayData = RibbonEmitterData->ReplayData;
		if (ReplayData.RenderPoints.empty() || ReplayData.TrailRanges.empty())
		{
			continue;
		}

		UMaterialInterface* RibbonMaterial = ResolveRibbonParticleMaterial(EmitterData->Material);
		const uint32 FirstInstance = static_cast<uint32>(RibbonInstances.size());
		FBoundingBox Bounds;

		for (const FRibbonRenderRange& Range : ReplayData.TrailRanges)
		{
			if (Range.PointCount < 2 || Range.PointStart < 0)
			{
				continue;
			}

			const int32 RangeEnd = Range.PointStart + Range.PointCount;
			if (RangeEnd > static_cast<int32>(ReplayData.RenderPoints.size()))
			{
				continue;
			}

			TArray<FVector> PointSides;
			PointSides.reserve(Range.PointCount);
			const bool bBillboardFacing = ReplayData.RibbonFacingMode != EParticleRibbonFacingMode::SourceTransform;
			for (int32 PointIndex = Range.PointStart; PointIndex < RangeEnd; ++PointIndex)
			{
				FVector PointSide = ResolveRibbonPointSide(
					ReplayData,
					EmitterData->ComponentToWorld,
					Range,
					PointIndex,
					Context.RenderBus.GetCameraPosition(),
					Context.RenderBus.GetCameraRight());
				if (bBillboardFacing &&
					!PointSides.empty() &&
					FVector::DotProduct(PointSide, PointSides.back()) < 0.0f)
				{
					PointSide = PointSide * -1.0f;
				}
				PointSides.push_back(PointSide);
			}

			for (int32 PointIndex = Range.PointStart; PointIndex + 1 < RangeEnd; ++PointIndex)
			{
				const FRibbonRenderPoint& A = ReplayData.RenderPoints[static_cast<size_t>(PointIndex)];
				const FRibbonRenderPoint& B = ReplayData.RenderPoints[static_cast<size_t>(PointIndex + 1)];
				const FVector Start = GetRibbonWorldPoint(ReplayData, EmitterData->ComponentToWorld, A);
				const FVector End = GetRibbonWorldPoint(ReplayData, EmitterData->ComponentToWorld, B);

				FParticleRibbonSegmentInstanceData Instance;
				Instance.Start = Start;
				Instance.End = End;
				Instance.HalfWidthStart = std::max(A.Width, 0.1f) * 0.5f;
				Instance.HalfWidthEnd = std::max(B.Width, 0.1f) * 0.5f;
				Instance.StartColor = A.Color;
				Instance.EndColor = B.Color;
				Instance.UVStartEnd = FVector2(A.U, B.U);
				const int32 SideIndex = PointIndex - Range.PointStart;
				Instance.StartSide = PointSides[static_cast<size_t>(SideIndex)];
				Instance.EndSide = PointSides[static_cast<size_t>(SideIndex + 1)];
				RibbonInstances.push_back(Instance);

				const FVector StartExtent(Instance.HalfWidthStart, Instance.HalfWidthStart, Instance.HalfWidthStart);
				const FVector EndExtent(Instance.HalfWidthEnd, Instance.HalfWidthEnd, Instance.HalfWidthEnd);
				Bounds.Expand(Start - StartExtent);
				Bounds.Expand(Start + StartExtent);
				Bounds.Expand(End - EndExtent);
				Bounds.Expand(End + EndExtent);
			}
		}

		const uint32 InstanceCount = static_cast<uint32>(RibbonInstances.size()) - FirstInstance;
		if (InstanceCount == 0)
		{
			continue;
		}

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Particle;
		Cmd.SourcePrimitive = Component;
		Cmd.Material = RibbonMaterial;
		Cmd.ParticleEmitterData = EmitterData;
		Cmd.ParticleReplayData = &ReplayData;
		Cmd.VertexFactoryType = EVertexFactoryType::ParticleRibbon;
		Cmd.PerObjectConstants = FPerObjectConstants{ FMatrix::Identity, FColor::White().ToVector4() };
		Cmd.WorldAABB = Bounds.IsValid() ? Bounds : BuildRibbonWorldBounds(ReplayData, EmitterData->ComponentToWorld);
		if (!Cmd.WorldAABB.IsValid())
		{
			Cmd.WorldAABB = Component->GetWorldAABB();
		}
		Cmd.Constants.Particle.ComponentToWorld = FMatrix::Identity;
		Cmd.Constants.Particle.CameraRight = Context.RenderBus.GetCameraRight();
		Cmd.Constants.Particle.CameraUp = Context.RenderBus.GetCameraUp();
		Cmd.Constants.Particle.EmitterType = static_cast<uint32>(EmitterData->GetEmitterType());
		Cmd.Constants.Particle.CoordinateSpace = static_cast<uint32>(EParticleCoordinateSpace::World);
		Cmd.Constants.Particle.ActiveParticleCount = InstanceCount;
		Cmd.Constants.Particle.bUseLocalSpace = 0u;
		Cmd.InstanceBufferView.InstanceCount = InstanceCount;
		Cmd.InstanceBufferView.Stride = sizeof(FParticleRibbonSegmentInstanceData);
		Cmd.InstanceBufferView.Offset = FirstInstance * sizeof(FParticleRibbonSegmentInstanceData);

		if (ResolveRibbonParticleRenderPass(Cmd.Material) == ERenderPass::Translucent)
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

bool FParticleSystemSceneProxy::EnsureSpriteInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
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

bool FParticleSystemSceneProxy::EnsureMeshInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
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

bool FParticleSystemSceneProxy::EnsureBeamInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
{
	if (Device == nullptr)
	{
		return false;
	}

	if (BeamInstanceBuffer && InstanceCount <= MaxBeamInstanceCount)
	{
		return true;
	}

	MaxBeamInstanceCount = std::max(InstanceCount * 2u, 1u);
	BeamInstanceBuffer.Reset();

	D3D11_BUFFER_DESC InstanceDesc = {};
	InstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	InstanceDesc.ByteWidth = sizeof(FBeamParticleInstanceData) * MaxBeamInstanceCount;
	InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(Device->CreateBuffer(&InstanceDesc, nullptr, BeamInstanceBuffer.ReleaseAndGetAddressOf()));
}

bool FParticleSystemSceneProxy::EnsureRibbonInstanceBuffer(ID3D11Device* Device, uint32 InstanceCount)
{
	if (Device == nullptr)
	{
		return false;
	}

	if (RibbonInstanceBuffer && InstanceCount <= MaxRibbonInstanceCount)
	{
		return true;
	}

	MaxRibbonInstanceCount = std::max(InstanceCount * 2u, 1u);
	RibbonInstanceBuffer.Reset();

	D3D11_BUFFER_DESC InstanceDesc = {};
	InstanceDesc.Usage = D3D11_USAGE_DYNAMIC;
	InstanceDesc.ByteWidth = sizeof(FParticleRibbonSegmentInstanceData) * MaxRibbonInstanceCount;
	InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	InstanceDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(Device->CreateBuffer(&InstanceDesc, nullptr, RibbonInstanceBuffer.ReleaseAndGetAddressOf()));
}

bool FParticleSystemSceneProxy::UploadSpriteInstances(ID3D11DeviceContext* DeviceContext)
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

bool FParticleSystemSceneProxy::UploadMeshInstances(ID3D11DeviceContext* DeviceContext)
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

bool FParticleSystemSceneProxy::UploadBeamInstances(ID3D11DeviceContext* DeviceContext)
{
	if (DeviceContext == nullptr || !BeamInstanceBuffer)
	{
		return false;
	}

	if (BeamInstances.empty())
	{
		return true;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(BeamInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	std::memcpy(
		Mapped.pData,
		BeamInstances.data(),
		sizeof(FBeamParticleInstanceData) * BeamInstances.size());
	DeviceContext->Unmap(BeamInstanceBuffer.Get(), 0);
	return true;
}


bool FParticleSystemSceneProxy::UploadRibbonInstances(ID3D11DeviceContext* DeviceContext)
{
	if (DeviceContext == nullptr || !RibbonInstanceBuffer)
	{
		return false;
	}

	if (RibbonInstances.empty())
	{
		return true;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(RibbonInstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	std::memcpy(
		Mapped.pData,
		RibbonInstances.data(),
		sizeof(FParticleRibbonSegmentInstanceData) * RibbonInstances.size());
	DeviceContext->Unmap(RibbonInstanceBuffer.Get(), 0);
	return true;
}
