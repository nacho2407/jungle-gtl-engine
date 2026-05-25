#include "Render/Proxy/ParticleDynamicEmitterData.h"

#include "Particles/ParticleSystem.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>

EParticleRenderType GetEmitterRenderType(const FParticleEmitterInstance* Instance)
{
	const UParticleLODLevel* LODLevel = Instance ? Instance->GetCurrentLODLevel() : nullptr;
	const UParticleModuleTypeDataBase* TypeData = LODLevel ? LODLevel->GetTypeDataModule() : nullptr;
	return TypeData ? TypeData->GetRenderType() : EParticleRenderType::Sprite;
}

FDynamicEmitterDataBase::FDynamicEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: EmitterIndex(InEmitterIndex)
	, Source(InSource)
{
}

const FDynamicEmitterReplayDataBase& FDynamicEmitterDataBase::GetSource() const
{
	return Source;
}

FDynamicSpriteEmitterDataBase::FDynamicSpriteEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort)
	: FDynamicEmitterDataBase(InEmitterIndex, InSource)
	, bDepthSort(bInDepthSort)
{
}

void FDynamicSpriteEmitterDataBase::GatherAliveParticleIndices(TArray<uint16>& OutRenderOrder) const
{
	OutRenderOrder.clear();

	const FParticleEmitterInstance* Instance = Source.Instance;
	if (!Instance || !Instance->ParticleIndices) return;

	const int32 ActiveCount = Instance->GetActiveParticleCount();
	const FParticleDataContainer& Data = Instance->GetParticleDataContainer();
	OutRenderOrder.reserve(ActiveCount);

	for (int32 Index = 0; Index < ActiveCount; ++Index)
	{
		const uint16 ParticleSlot = Instance->ParticleIndices[Index];
		if (Data.GetParticle(ParticleSlot).bAlive)
		{
			OutRenderOrder.push_back(ParticleSlot);
		}
	}
}

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(const FFrameContext& Frame, TArray<uint16>& RenderOrder) const
{
	if (!bDepthSort || RenderOrder.size() < 2) return;

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	std::sort(RenderOrder.begin(), RenderOrder.end(),
		[&Data, &Frame](uint16 A, uint16 B)
		{
			const FBaseParticle& ParticleA = Data.GetParticle(A);
			const FBaseParticle& ParticleB = Data.GetParticle(B);

			const float DepthA = (ParticleA.Position - Frame.CameraPosition).Dot(Frame.CameraForward);
			const float DepthB = (ParticleB.Position - Frame.CameraPosition).Dot(Frame.CameraForward);
			return DepthA > DepthB;
		});
}

uint32 FDynamicSpriteEmitterDataBase::BuildDynamicVertexData(const FFrameContext& Frame, FSpriteParticleGeometry& OutGeometry) const
{
	if (!Source.Instance) return 0;

	TArray<uint16> RenderOrder;
	GatherAliveParticleIndices(RenderOrder);
	SortSpriteParticles(Frame, RenderOrder);

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	const uint32 FirstIndex = OutGeometry.GetIndexCount();
	for (uint16 ParticleSlot : RenderOrder)
	{
		OutGeometry.AddParticleQuad(Data.GetParticle(ParticleSlot), Frame.CameraRight, Frame.CameraUp);
	}

	return OutGeometry.GetIndexCount() - FirstIndex;
}

FDynamicSpriteEmitterData::FDynamicSpriteEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, bInDepthSort)
{
}

int32 FDynamicSpriteEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

FDynamicRibbonEmitterData::FDynamicRibbonEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, false)
{
}

int32 FDynamicRibbonEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

FDynamicBeamEmitterData::FDynamicBeamEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, false)
{
}

int32 FDynamicBeamEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

FDynamicMeshEmitterData::FDynamicMeshEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, FParticleMeshEmitterInstance* InMeshInstance)
	: FDynamicEmitterDataBase(InEmitterIndex, InSource)
	, MeshInstance(InMeshInstance)
{
}

int32 FDynamicMeshEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FMeshParticleInstanceData);
}

uint32 FDynamicMeshEmitterData::BuildDynamicVertexData(TArray<FMeshParticleInstanceData>& OutInstances) const
{
	if (!Source.Instance || !Source.Instance->ParticleIndices || !MeshInstance) return 0;

	const uint32 FirstInstance = static_cast<uint32>(OutInstances.size());
	const int32 ActiveCount = Source.Instance->GetActiveParticleCount();
	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();

	for (int32 ParticleIndex = 0; ParticleIndex < ActiveCount; ++ParticleIndex)
	{
		const FBaseParticle& Particle = Data.GetParticle(Source.Instance->ParticleIndices[ParticleIndex]);
		if (!Particle.bAlive) continue;

		FMeshParticleTransform Transform;
		Transform.Position = Particle.Position;
		Transform.Scale = Particle.Size;
		Transform.RotationEuler = FVector(0, 0, Particle.Rotation);

		if (const FParticleMeshPayload* Payload = MeshInstance->GetParticlePayload<FParticleMeshPayload>(ParticleIndex))
		{
			Transform.Scale = FVector(
				Transform.Scale.X * Payload->MeshScale.X,
				Transform.Scale.Y * Payload->MeshScale.Y,
				Transform.Scale.Z * Payload->MeshScale.Z);
			Transform.RotationEuler += Payload->MeshRotation;
		}

		OutInstances.push_back(MakeMeshParticleInstanceData(Transform, Particle.Color));
	}

	return static_cast<uint32>(OutInstances.size()) - FirstInstance;
}
