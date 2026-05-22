#include "Particles/Runtime/ParticleEmitterInstance.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/Module/ParticleModule.h"

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

void FParticleEmitterInstance::Reset()
{
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		GetParticle(Index).bAlive = false;
		ParticleIndices[Index] = 0;
	}

	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	EmitterTime = 0.0f;
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
		FBaseParticle* Particle = SpawnParticle();
		if (!Particle)
		{
			break;
		}

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
	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles;)
	{
		FBaseParticle& Particle = GetParticle(ParticleIndex);
		Particle.Age += DeltaTime;

		if (Particle.Age >= Particle.Lifetime)
		{
			KillParticle(ParticleIndex);
			continue;
		}

		Particle.OldPosition = Particle.Position;
		Particle.Position += Particle.Velocity * DeltaTime;
		Particle.Rotation += Particle.RotationRate * DeltaTime;
		Particle.RelativeTime = Particle.Age * Particle.OneOverMaxLifetime;
		RunUpdateModules(Particle, DeltaTime);
		++ParticleIndex;
	}
}

void FParticleEmitterInstance::KillParticle(int32 ParticleIndex)
{
	if (ParticleIndex < 0 || ParticleIndex >= ActiveParticles)
	{
		return;
	}

	const int32 LastParticleIndex = ActiveParticles - 1;
	GetParticle(ParticleIndex).bAlive = false;

	if (ParticleIndex != LastParticleIndex)
	{
		GetParticle(ParticleIndex) = GetParticle(LastParticleIndex);
		ParticleIndices[ParticleIndex] = static_cast<uint16>(ParticleIndex);
	}

	GetParticle(LastParticleIndex).bAlive = false;
	ParticleIndices[LastParticleIndex] = 0;
	--ActiveParticles;
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

	const int32 ParticleIndex = ActiveParticles++;
	ParticleIndices[ParticleIndex] = static_cast<uint16>(ParticleIndex);

	FBaseParticle& Particle = GetParticle(ParticleIndex);
	Particle = FBaseParticle();
	Particle.bAlive = true;
	return &Particle;
}

FBaseParticle& FParticleEmitterInstance::GetParticle(int32 ParticleIndex)
{
	return *reinterpret_cast<FBaseParticle*>(ParticleData + ParticleIndex * ParticleStride);
}

const FBaseParticle& FParticleEmitterInstance::GetParticle(int32 ParticleIndex) const
{
	return *reinterpret_cast<const FBaseParticle*>(ParticleData + ParticleIndex * ParticleStride);
}

UParticleModuleRequired* FParticleEmitterInstance::GetRequiredModule() const
{
	return CurrentLODLevel ? CurrentLODLevel->FindModule<UParticleModuleRequired>() : nullptr;
}

UParticleModuleSpawn* FParticleEmitterInstance::GetSpawnModule() const
{
	return CurrentLODLevel ? CurrentLODLevel->FindModule<UParticleModuleSpawn>() : nullptr;

}

void FParticleEmitterInstance::RunSpawnModules(FBaseParticle& Particle, float SpawnTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (Module && Module->IsSpawnModule())
		{
			Module->Spawn(this, PayloadOffset, SpawnTime, Particle);
		}
	}
}

void FParticleEmitterInstance::RunUpdateModules(FBaseParticle& Particle, float DeltaTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}
	for(UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (Module && Module->IsUpdateModule())
		{
			Module->Update(this, PayloadOffset, DeltaTime, Particle);
		}
	}
}
