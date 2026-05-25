#include "Particles/Runtime/ParticleEmitterInstance.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleHelper.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"

#include <algorithm>

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	ReleaseParticleData();
}

void FParticleEmitterInstance::Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	SpriteTemplate = InTemplate;
	Component = InComponent;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(CurrentLODLevelIndex) : nullptr;
	AllocateParticleData(SpriteTemplate ? SpriteTemplate->GetMaxActiveParticles() : 0);
	Reset();
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!bActive || !SpriteTemplate)
	{
		return;
	}

	CollisionEventQueue.clear();
	EmitterTime += DeltaTime;

	const float Duration = SpriteTemplate->GetEmitterDuration();
	if (Duration > 0.0f && EmitterTime >= Duration)
	{
		if (SpriteTemplate->IsLooping())
		{
			while (EmitterTime >= Duration)
			{
				EmitterTime -= Duration;
			}
		}
		else
		{
			EmitterTime = Duration;
			bActive = false;
		}
	}

	SpawnParticles(DeltaTime);
	UpdateParticles(DeltaTime);
}

void FParticleEmitterInstance::SetLODLevelIndex(int32 LODLevelIndex)
{
	if (!SpriteTemplate)
	{
		return;
	}

	const int32 LODCount = static_cast<int32>(SpriteTemplate->GetLODLevels().size());
	if (LODCount <= 0)
	{
		return;
	}

	int32 NewLODLevelIndex = (std::max)(0, (std::min)(LODLevelIndex, LODCount - 1));
	UParticleLODLevel* NewLODLevel = SpriteTemplate->GetLODLevel(NewLODLevelIndex);
	if (!CanUseLODLevel(NewLODLevel))
	{
		for (int32 CandidateIndex = NewLODLevelIndex - 1; CandidateIndex >= 0; --CandidateIndex)
		{
			UParticleLODLevel* CandidateLODLevel = SpriteTemplate->GetLODLevel(CandidateIndex);
			if (CanUseLODLevel(CandidateLODLevel))
			{
				NewLODLevelIndex = CandidateIndex;
				NewLODLevel = CandidateLODLevel;
				break;
			}
		}
	}

	if (!CanUseLODLevel(NewLODLevel) || NewLODLevelIndex == CurrentLODLevelIndex)
	{
		return;
	}

	CurrentLODLevelIndex = NewLODLevelIndex;
	CurrentLODLevel = NewLODLevel;
}

void FParticleEmitterInstance::Reset()
{
	for (int32 Index = 0; Index < MaxActiveParticles; ++Index)
	{
		GetParticleBySlot(Index).bAlive = false;
		ParticleIndices[Index] = static_cast<uint16>(Index);
	}

	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	EmitterTime = 0.0f;
	CollisionEventQueue.clear();
	bActive = true;
}

int32 FParticleEmitterInstance::SpawnParticles(float DeltaTime)
{
	if (DeltaTime <= 0.0f || ActiveParticles >= MaxActiveParticles)
	{
		return 0;
	}

	const float EffectiveSpawnRate = GetSpawnModule() ? GetSpawnModule()->SpawnRate : SpawnRate;
	SpawnFraction += EffectiveSpawnRate * DeltaTime;
	const int32 SpawnCount = static_cast<int32>(SpawnFraction);
	if (SpawnCount <= 0)
	{
		return 0;
	}

	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		if (!ParticleData || ActiveParticles >= MaxActiveParticles)
		{
			break;
		}

		const int32 ParticleListIndex = ActiveParticles++;
		const int32 ParticleSlot = ParticleIndices[ParticleListIndex];

		uint8* ParticleData = this->ParticleData + ParticleSlot * ParticleStride;
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData);

		*Particle = FBaseParticle();
		Particle->bAlive = true;

		InitializeParticle(*Particle);
		++ParticleCounter;
		++SpawnedCount;
	}

	SpawnFraction -= static_cast<float>(SpawnedCount);
	return SpawnedCount;
}

void FParticleEmitterInstance::InitializeParticle(FBaseParticle& Particle)
{
	const FVector SpawnLocation = Component ? Component->GetWorldLocation() : FVector::ZeroVector;

	Particle.Position = SpawnLocation;
	Particle.OldPosition = SpawnLocation;
	Particle.Velocity = DefaultVelocity;
	Particle.Size = DefaultSize;
	Particle.Rotation = 0.0f;
	Particle.RotationRate = 0.0f;
	Particle.Color = DefaultColor;
	Particle.Lifetime = DefaultLifetime;
	Particle.Age = 0.0f;
	Particle.RelativeTime = 0.0f;
	Particle.OneOverMaxLifetime = DefaultLifetime > 0.0f ? 1.0f / DefaultLifetime : 1.0f;
	Particle.FrameIndex = ParticleCounter;
	Particle.bAlive = true;

	RunSpawnModules(Particle, EmitterTime);
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	struct
	{
		FParticleEmitterInstance& Owner;
		int32 Offset;
		float DeltaTime;
	} Context{ *this, 0, DeltaTime };

	BEGIN_UPDATE_LOOP
		Particle->Age += DeltaTime;

		if (Particle->Age >= Particle->Lifetime)
		{
			Particle->bAlive = false;
		}

		if (Particle->bAlive)
		{
			Particle->OldPosition = Particle->Position;
			Particle->Position += Particle->Velocity * DeltaTime;
			Particle->Rotation += Particle->RotationRate * DeltaTime;
			Particle->RelativeTime = Particle->Age * Particle->OneOverMaxLifetime;
		}
	END_UPDATE_LOOP

	RunUpdateModules(DeltaTime);
	if (Component && bIsEventGenerator)
	{
		for (const FParticleCollisionEventPayload& Event : CollisionEventQueue)
		{
			Component->BroadcastParticleCollisionEvent(Event);
		}
	}
	CompactDeadParticles();
}

void FParticleEmitterInstance::CompactDeadParticles()
{
	for (int32 ParticleIndex = 0; ParticleIndex < this->ActiveParticles;)
	{
		FBaseParticle& Particle = GetParticle(ParticleIndex);
		if (!Particle.bAlive)
		{
			KillParticle(ParticleIndex);
			continue;
		}

		++ParticleIndex;
	}
}

void FParticleEmitterInstance::KillParticle(int32 ParticleIndex)
{
	if (ParticleIndex < 0 || ParticleIndex >= ActiveParticles)
	{
		return;
	}

	const int32 ParticleSlot = ParticleIndices[ParticleIndex];
	const int32 LastParticleIndex = ActiveParticles - 1;
	GetParticleBySlot(ParticleSlot).bAlive = false;

	if (ParticleIndex != LastParticleIndex)
	{
		ParticleIndices[ParticleIndex] = ParticleIndices[LastParticleIndex];
	}

	--ActiveParticles;
	ParticleIndices[ActiveParticles] = static_cast<uint16>(ParticleSlot);
}

void FParticleEmitterInstance::AllocateParticleData(int32 InMaxActiveParticles)
{
	const int32 RequestedInstancePayloadSize = InstancePayloadSize;
	ReleaseParticleData();
	InstancePayloadSize = RequestedInstancePayloadSize;

	MaxActiveParticles = InMaxActiveParticles > 0 ? InMaxActiveParticles : 0;
	if (MaxActiveParticles > 65535)
	{
		MaxActiveParticles = 65535;
	}

	PayloadOffset = static_cast<int32>(sizeof(FBaseParticle));
	ParticleSize = PayloadOffset + InstancePayloadSize;
	ParticleStride = ParticleSize;
	ParticleDataContainer.Initialize(MaxActiveParticles, ParticleStride);

	MemBlockSize = ParticleDataContainer.MemBlockSize;
	ParticleDataNumBytes = ParticleDataContainer.ParticleDataNumBytes;
	ParticleIndicesNumShorts = ParticleDataContainer.ParticleIndicesNumShorts;
	ParticleData = ParticleDataContainer.ParticleData;
	ParticleIndices = ParticleDataContainer.ParticleIndices;
}

void FParticleEmitterInstance::ReleaseParticleData()
{
	delete[] InstanceData;
	ParticleDataContainer.Release();

	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceData = nullptr;
	InstancePayloadSize = 0;
	PayloadOffset = 0;
	ParticleSize = 0;
	ParticleStride = 0;
	ActiveParticles = 0;
	MaxActiveParticles = 0;
	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
}

FBaseParticle* FParticleEmitterInstance::SpawnParticle()
{
	if (!ParticleData || ActiveParticles >= MaxActiveParticles)
	{
		return nullptr;
	}

	const int32 ParticleListIndex = ActiveParticles++;
	const int32 ParticleSlot = ParticleIndices[ParticleListIndex];

	FBaseParticle& Particle = GetParticleBySlot(ParticleSlot);
	Particle = FBaseParticle();
	Particle.bAlive = true;
	return &Particle;
}

FBaseParticle& FParticleEmitterInstance::GetParticle(int32 ParticleIndex)
{
	return GetParticleBySlot(ParticleIndices[ParticleIndex]);
}

const FBaseParticle& FParticleEmitterInstance::GetParticle(int32 ParticleIndex) const
{
	return GetParticleBySlot(ParticleIndices[ParticleIndex]);
}

FBaseParticle& FParticleEmitterInstance::GetParticleBySlot(int32 ParticleSlot)
{
	return *reinterpret_cast<FBaseParticle*>(ParticleData + ParticleSlot * ParticleStride);
}

const FBaseParticle& FParticleEmitterInstance::GetParticleBySlot(int32 ParticleSlot) const
{
	return *reinterpret_cast<const FBaseParticle*>(ParticleData + ParticleSlot * ParticleStride);
}

UParticleModuleRequired* FParticleEmitterInstance::GetRequiredModule() const
{
	return CurrentLODLevel ? CurrentLODLevel->FindModule<UParticleModuleRequired>() : nullptr;
}

UParticleModuleSpawn* FParticleEmitterInstance::GetSpawnModule() const
{
	return CurrentLODLevel ? CurrentLODLevel->FindModule<UParticleModuleSpawn>() : nullptr;

}

bool FParticleEmitterInstance::CanUseLODLevel(const UParticleLODLevel* LODLevel) const
{
	if (!LODLevel || !LODLevel->IsEnabled())
	{
		return false;
	}

	const UParticleModuleTypeDataBase* CurrentTypeData = CurrentLODLevel ? CurrentLODLevel->GetTypeDataModule() : nullptr;
	const UParticleModuleTypeDataBase* NewTypeData = LODLevel->GetTypeDataModule();
	const EParticleRenderType CurrentRenderType = CurrentTypeData ? CurrentTypeData->GetRenderType() : EParticleRenderType::Sprite;
	const EParticleRenderType NewRenderType = NewTypeData ? NewTypeData->GetRenderType() : EParticleRenderType::Sprite;
	if (CurrentRenderType != NewRenderType)
	{
		return false;
	}

	const int32 NewPayloadSize = NewTypeData ? NewTypeData->GetParticlePayloadSize() : 0;
	return NewPayloadSize == InstancePayloadSize;
}

void FParticleEmitterInstance::RunSpawnModules(FBaseParticle& Particle, float SpawnTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	if (UParticleModuleTypeDataBase* TypeDataModule = CurrentLODLevel->GetTypeDataModule())
	{
		if (TypeDataModule->IsSpawnModule())
		{
			TypeDataModule->Spawn(this, PayloadOffset, SpawnTime, Particle);
		}
	}

	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (Module && Module->IsSpawnModule())
		{
			Module->Spawn(this, PayloadOffset, SpawnTime, Particle);
		}
	}
}

void FParticleEmitterInstance::RunUpdateModules(float DeltaTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	if (UParticleModuleTypeDataBase* TypeDataModule = CurrentLODLevel->GetTypeDataModule())
	{
		if (TypeDataModule->IsUpdateModule())
		{
			TypeDataModule->Update(this, PayloadOffset, DeltaTime);
		}
	}

	for(UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (Module && Module->IsUpdateModule())
		{
			Module->Update(this, PayloadOffset, DeltaTime);
		}
	}
}
