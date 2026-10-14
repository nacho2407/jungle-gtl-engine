#include "Particles/Runtime/ParticleEmitterInstance.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleHelper.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleEvent.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"

#include <algorithm>
#include <cstring>

namespace
{
	FVector GetSafeNormalizedParticleAxis(const FVector& Axis, const FVector& Fallback)
	{
		const float Length = Axis.Length();
		if (Length <= 1.0e-6f)
		{
			return Fallback;
		}

		return Axis / Length;
	}
}

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
	if (!SpriteTemplate)
	{
		return;
	}

	if (!bActive && ActiveParticles <= 0)
	{
		return;
	}

	CollisionEventQueue.clear();
	RefreshEventGeneratorFlags();
	EmitterTime += DeltaTime;

	const UParticleModuleRequired* RequiredModule = GetRequiredModule();
	const float Duration = RequiredModule ? RequiredModule->EmitterDuration : SpriteTemplate->GetEmitterDuration();
	if (Duration > 0.0f && EmitterTime >= Duration)
	{
		const bool bLooping = RequiredModule ? RequiredModule->bLooping : SpriteTemplate->IsLooping();
		if (bLooping)
		{
			bool bLooped = false;
			while (EmitterTime >= Duration)
			{
				EmitterTime -= Duration;
				bLooped = true;
			}

			const UParticleModuleSpawn* SpawnModule = GetSpawnModule();
			if (bLooped && (!SpawnModule || SpawnModule->SpawnType != EParticleSpawnType::Burst || SpawnModule->bRepeatBurstOnLoop))
			{
				bSpawnBurstFired = false;
			}
		}
		else
		{
			EmitterTime = Duration;
			bSpawningEnabled = false;
		}
	}

	SpawnParticles(DeltaTime);
	UpdateParticles(DeltaTime);

	if (!bSpawningEnabled && ActiveParticles <= 0)
	{
		bActive = false;
	}
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
	if (NewLODLevel && !NewLODLevel->IsEnabled())
	{
		CurrentLODLevelIndex = NewLODLevelIndex;
		CurrentLODLevel = NewLODLevel;
		bSpawnBurstFired = false;
		return;
	}

	if (!CanUseLODLevel(NewLODLevel))
	{
		for (int32 CandidateIndex = NewLODLevelIndex - 1; CandidateIndex >= 0; --CandidateIndex)
		{
			UParticleLODLevel* CandidateLODLevel = SpriteTemplate->GetLODLevel(CandidateIndex);
			if (CandidateLODLevel && CandidateLODLevel->IsEnabled() && CanUseLODLevel(CandidateLODLevel))
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
	bSpawnBurstFired = false;
}

float FParticleEmitterInstance::GetParticleScaleMultiplier() const
{
	return Component ? (std::max)(0.0f, Component->GetParticleScaleMultiplier()) : 1.0f;
}

FVector FParticleEmitterInstance::RotateLocalVectorToWorld(const FVector& LocalVector) const
{
	if (!Component)
	{
		return LocalVector;
	}

	const FMatrix& WorldMatrix = Component->GetWorldMatrix();
	const FVector XAxis = GetSafeNormalizedParticleAxis(
		FVector(WorldMatrix.M[0][0], WorldMatrix.M[0][1], WorldMatrix.M[0][2]),
		FVector(1.0f, 0.0f, 0.0f));
	const FVector YAxis = GetSafeNormalizedParticleAxis(
		FVector(WorldMatrix.M[1][0], WorldMatrix.M[1][1], WorldMatrix.M[1][2]),
		FVector(0.0f, 1.0f, 0.0f));
	const FVector ZAxis = GetSafeNormalizedParticleAxis(
		FVector(WorldMatrix.M[2][0], WorldMatrix.M[2][1], WorldMatrix.M[2][2]),
		FVector(0.0f, 0.0f, 1.0f));

	return XAxis * LocalVector.X + YAxis * LocalVector.Y + ZAxis * LocalVector.Z;
}

FVector FParticleEmitterInstance::TransformLocalOffsetToWorld(const FVector& WorldOrigin, const FVector& LocalOffset) const
{
	return WorldOrigin + RotateLocalVectorToWorld(LocalOffset);
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
	bSpawnBurstFired = false;
	CollisionEventQueue.clear();
	bIsEventGenerator = false;
	bGenerateSpawnEvents = false;
	bGenerateKillEvents = false;
	bGenerateCollisionEvents = false;
	SpawnEventName = FName("Spawn");
	KillEventName = FName("Kill");
	CollisionEventName = FName("Collision");
	bActive = true;
	bSpawningEnabled = true;
}

int32 FParticleEmitterInstance::SpawnParticles(float DeltaTime)
{
	if (!bSpawningEnabled || DeltaTime <= 0.0f || ActiveParticles >= MaxActiveParticles)
	{
		return 0;
	}

	const UParticleModuleSpawn* SpawnModule = GetSpawnModule();
	const EParticleSpawnType SpawnType = SpawnModule ? SpawnModule->SpawnType : EParticleSpawnType::Default;
	const float EffectiveSpawnRate = SpawnModule
		? SpawnModule->SpawnRate.GetValue(EmitterTime, 0.5f)
		: SpawnRate;

	if (SpawnType == EParticleSpawnType::Default)
	{
		SpawnFraction += EffectiveSpawnRate * DeltaTime;
	}

	int32 BurstSpawnCount = 0;
	if (SpawnModule && SpawnType == EParticleSpawnType::Burst && !bSpawnBurstFired && SpawnModule->BurstCount > 0 && EmitterTime >= SpawnModule->BurstTime)
	{
		BurstSpawnCount = SpawnModule->BurstCount;
		bSpawnBurstFired = true;
	}

	const int32 RateSpawnCount = SpawnType == EParticleSpawnType::Default ? static_cast<int32>(SpawnFraction) : 0;
	const int32 SpawnCount = BurstSpawnCount + RateSpawnCount;
	if (SpawnCount <= 0)
	{
		return 0;
	}

	const int32 SpawnedBurstCount = SpawnParticleCount(BurstSpawnCount);
	const int32 SpawnedRateCount = SpawnParticleCount(RateSpawnCount);
	SpawnFraction -= static_cast<float>(SpawnedRateCount);
	return SpawnedBurstCount + SpawnedRateCount;
}

int32 FParticleEmitterInstance::SpawnParticleCount(int32 RequestedSpawnCount)
{
	if (RequestedSpawnCount <= 0)
	{
		return 0;
	}

	int32 SpawnedCount = 0;
	for (int32 Index = 0; Index < RequestedSpawnCount; ++Index)
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

		const uint32 SpawnSerial = static_cast<uint32>(Index);
		Particle->FrameIndex = ParticleCounter++;
		Particle->RandomSeed = BuildParticleRandomSeed(Particle->FrameIndex, SpawnSerial);
		InitializeParticle(*Particle);
		++SpawnedCount;
	}

	return SpawnedCount;
}

uint32 FParticleEmitterInstance::BuildParticleRandomSeed(uint32 ParticleSerial, uint32 SpawnSerial) const
{
	uint32 EmitterTimeBits = 0;
	std::memcpy(&EmitterTimeBits, &EmitterTime, sizeof(EmitterTimeBits));

	uint32 EmitterSeed = FDistributionSampling::Hash(EmitterTimeBits);
	EmitterSeed = FDistributionSampling::Hash(EmitterSeed ^ FDistributionSampling::Hash(CurrentLODLevelIndex + 0x27d4eb2du));
	EmitterSeed = FDistributionSampling::Hash(EmitterSeed ^ FDistributionSampling::Hash(static_cast<uint32>(ActiveParticles)));

	return FDistributionSampling::RandomSeed(EmitterSeed, ParticleSerial, SpawnSerial);
}

void FParticleEmitterInstance::InitializeParticle(FBaseParticle& Particle)
{
	const FVector SpawnLocation = Component ? Component->GetWorldLocation() : FVector::ZeroVector;
	InitializeParticle(Particle, SpawnLocation);
}

void FParticleEmitterInstance::InitializeParticle(FBaseParticle& Particle, const FVector& SpawnLocation)
{
	Particle.Position = SpawnLocation;
	Particle.OldPosition = SpawnLocation;
	Particle.Velocity = RotateLocalVectorToWorld(DefaultVelocity);
	Particle.Size = DefaultSize;
	Particle.InitialSize = DefaultSize;
	Particle.TargetSize = DefaultSize;
	Particle.Rotation = 0.0f;
	Particle.RotationRate = 0.0f;
	Particle.Color = DefaultColor;
	Particle.Lifetime = DefaultLifetime;
	Particle.Age = 0.0f;
	Particle.RelativeTime = 0.0f;
	Particle.OneOverMaxLifetime = DefaultLifetime > 0.0f ? 1.0f / DefaultLifetime : 1.0f;
	if (Particle.RandomSeed == 0)
	{
		Particle.FrameIndex = ParticleCounter++;
		Particle.RandomSeed = BuildParticleRandomSeed(Particle.FrameIndex, 0);
	}
	Particle.bAlive = true;

	RunSpawnModules(Particle, EmitterTime);

	if (bGenerateSpawnEvents)
	{
		QueueParticleEvent(EParticleEventType::Spawn, SpawnEventName, Particle, static_cast<int32>(Particle.FrameIndex));
	}
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
	CompactDeadParticles();
	if (Component && bIsEventGenerator)
	{
		for (const FParticleCollisionEventPayload& Event : CollisionEventQueue)
		{
			Component->BroadcastParticleEvent(Event);
		}
	}
}

void FParticleEmitterInstance::CompactDeadParticles()
{
	for (int32 ParticleIndex = 0; ParticleIndex < this->ActiveParticles;)
	{
		FBaseParticle& Particle = GetParticle(ParticleIndex);
		if (!Particle.bAlive)
		{
			if (bGenerateKillEvents)
			{
				QueueParticleEvent(EParticleEventType::Kill, KillEventName, Particle, ParticleIndex);
			}
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

void FParticleEmitterInstance::QueueParticleEvent(EParticleEventType EventType, const FName& EventName, const FBaseParticle& Particle, int32 ParticleIndex)
{
	if (!bIsEventGenerator)
	{
		return;
	}

	FParticleCollisionEventPayload NewEvent;
	NewEvent.EventName = EventName;
	NewEvent.EventType = EventType;
	NewEvent.EmitterTime = EmitterTime;
	NewEvent.Location = Particle.Position;
	NewEvent.Normal = FVector::ZeroVector;
	NewEvent.Velocity = Particle.Velocity;
	NewEvent.Direction = Particle.Velocity.LengthSquared() > 0.0001f ? Particle.Velocity.Normalized() : FVector::ZeroVector;
	NewEvent.ParticleIndex = ParticleIndex;
	NewEvent.HitComponent = nullptr;
	CollisionEventQueue.push_back(NewEvent);
}

void FParticleEmitterInstance::ReceiveParticleEvent(const FParticleCollisionEventPayload& Event)
{
	if (!CurrentLODLevel || !CurrentLODLevel->IsEnabled())
	{
		return;
	}

	const TArray<UParticleModule*>& Modules = CurrentLODLevel->GetModules();
	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		UParticleModule* Module = CurrentLODLevel->ResolveModule(ModuleIndex, SpriteTemplate);
		UParticleModuleEventReceiver* Receiver = Cast<UParticleModuleEventReceiver>(Module);
		if (Receiver && Receiver->IsEnabled())
		{
			Receiver->ReceiveEvent(this, Event);
		}
	}
}

void FParticleEmitterInstance::AllocateParticleData(int32 InMaxActiveParticles)
{
	const int32 RequestedInstancePayloadSize = InstancePayloadSize;
	const int32 RequestedInstancePayloadAlignment = InstancePayloadAlignment;
	ReleaseParticleData();
	InstancePayloadSize = RequestedInstancePayloadSize;
	InstancePayloadAlignment = RequestedInstancePayloadAlignment > 0
		? RequestedInstancePayloadAlignment
		: static_cast<int32>(alignof(FBaseParticle));

	MaxActiveParticles = InMaxActiveParticles > 0 ? InMaxActiveParticles : 0;
	if (MaxActiveParticles > 65535)
	{
		MaxActiveParticles = 65535;
	}

	const int32 BaseParticleAlignment = static_cast<int32>(alignof(FBaseParticle));
	const int32 ParticleAlignment = BaseParticleAlignment > InstancePayloadAlignment
		? BaseParticleAlignment
		: InstancePayloadAlignment;
	PayloadOffset = FParticleDataContainer::AlignUp(static_cast<int32>(sizeof(FBaseParticle)), InstancePayloadAlignment);
	ParticleSize = PayloadOffset + InstancePayloadSize;
	ParticleStride = FParticleDataContainer::AlignUp(ParticleSize, ParticleAlignment);
	ParticleDataContainer.Initialize(MaxActiveParticles, ParticleStride, ParticleAlignment);

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
	InstancePayloadAlignment = static_cast<int32>(alignof(FBaseParticle));
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
	Particle.FrameIndex = ParticleCounter++;
	Particle.RandomSeed = BuildParticleRandomSeed(Particle.FrameIndex, 0);
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
	UParticleModuleRequired* RequiredModule = CurrentLODLevel ? CurrentLODLevel->FindResolvedModule<UParticleModuleRequired>(SpriteTemplate) : nullptr;
	return RequiredModule && RequiredModule->IsEnabled() ? RequiredModule : nullptr;
}

UParticleModuleSpawn* FParticleEmitterInstance::GetSpawnModule() const
{
	UParticleModuleSpawn* SpawnModule = CurrentLODLevel ? CurrentLODLevel->FindResolvedModule<UParticleModuleSpawn>(SpriteTemplate) : nullptr;
	return SpawnModule && SpawnModule->IsEnabled() ? SpawnModule : nullptr;

}

namespace
{
	bool IsBeamOnlyModule(const UParticleModule* Module)
	{
		return Cast<UParticleModuleBeamSource>(Module)
			|| Cast<UParticleModuleBeamNoise>(Module)
			|| Cast<UParticleModuleBeamTarget>(Module);
	}
}

bool FParticleEmitterInstance::CanUseLODLevel(const UParticleLODLevel* LODLevel) const
{
	if (!LODLevel)
	{
		return false;
	}

	const UParticleModuleTypeDataBase* CurrentTypeData = CurrentLODLevel ? CurrentLODLevel->ResolveTypeDataModule(SpriteTemplate) : nullptr;
	const UParticleModuleTypeDataBase* NewTypeData = LODLevel->ResolveTypeDataModule(SpriteTemplate);
	const EParticleRenderType CurrentRenderType = CurrentTypeData ? CurrentTypeData->GetRenderType() : EParticleRenderType::Sprite;
	const EParticleRenderType NewRenderType = NewTypeData ? NewTypeData->GetRenderType() : EParticleRenderType::Sprite;
	if (CurrentRenderType != NewRenderType)
	{
		return false;
	}

	const int32 NewPayloadSize = NewTypeData ? NewTypeData->GetParticlePayloadSize() : 0;
	const int32 NewPayloadAlignment = NewTypeData ? NewTypeData->GetParticlePayloadAlignment() : static_cast<int32>(alignof(FBaseParticle));
	return NewPayloadSize == InstancePayloadSize && NewPayloadAlignment == InstancePayloadAlignment;
}

void FParticleEmitterInstance::RefreshEventGeneratorFlags()
{
	bIsEventGenerator = false;
	bGenerateSpawnEvents = false;
	bGenerateKillEvents = false;
	bGenerateCollisionEvents = false;
	SpawnEventName = FName("Spawn");
	KillEventName = FName("Kill");
	CollisionEventName = FName("Collision");

	if (!CurrentLODLevel)
	{
		return;
	}

	const TArray<UParticleModule*>& Modules = CurrentLODLevel->GetModules();
	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		UParticleModule* Module = CurrentLODLevel->ResolveModule(ModuleIndex, SpriteTemplate);
		UParticleModuleEventGenerator* Generator = Cast<UParticleModuleEventGenerator>(Module);
		if (!Generator || !Generator->IsEnabled())
		{
			continue;
		}

		bGenerateSpawnEvents = bGenerateSpawnEvents || Generator->bGenerateSpawnEvents;
		bGenerateKillEvents = bGenerateKillEvents || Generator->bGenerateKillEvents;
		bGenerateCollisionEvents = bGenerateCollisionEvents || Generator->bGenerateCollisionEvents;
		if (Generator->SpawnEventName.IsValid() && Generator->SpawnEventName != FName::None)
		{
			SpawnEventName = Generator->SpawnEventName;
		}
		if (Generator->KillEventName.IsValid() && Generator->KillEventName != FName::None)
		{
			KillEventName = Generator->KillEventName;
		}
		if (Generator->CollisionEventName.IsValid() && Generator->CollisionEventName != FName::None)
		{
			CollisionEventName = Generator->CollisionEventName;
		}
	}

	bIsEventGenerator = bGenerateSpawnEvents || bGenerateKillEvents || bGenerateCollisionEvents;
}

void FParticleEmitterInstance::RunSpawnModules(FBaseParticle& Particle, float SpawnTime)
{
	if (!CurrentLODLevel)
	{
		return;
	}

	UParticleModuleTypeDataBase* TypeDataModule = CurrentLODLevel->ResolveTypeDataModule(SpriteTemplate);
	const bool bCurrentTypeDataIsBeam = Cast<UParticleModuleTypeDataBeam>(TypeDataModule) != nullptr;
	if (TypeDataModule)
	{
		if (TypeDataModule->IsSpawnModule())
		{
			TypeDataModule->Spawn(this, PayloadOffset, SpawnTime, Particle);
		}
	}

	const TArray<UParticleModule*>& Modules = CurrentLODLevel->GetModules();
	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		UParticleModule* Module = CurrentLODLevel->ResolveModule(ModuleIndex, SpriteTemplate);
		if (IsBeamOnlyModule(Module) && !bCurrentTypeDataIsBeam)
		{
			continue;
		}
		if (Module && Module->IsEnabled() && Module->IsSpawnModule())
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

	UParticleModuleTypeDataBase* TypeDataModule = CurrentLODLevel->ResolveTypeDataModule(SpriteTemplate);
	const bool bCurrentTypeDataIsBeam = Cast<UParticleModuleTypeDataBeam>(TypeDataModule) != nullptr;
	if (TypeDataModule)
	{
		if (TypeDataModule->IsUpdateModule())
		{
			TypeDataModule->Update(this, PayloadOffset, DeltaTime);
		}
	}

	const TArray<UParticleModule*>& Modules = CurrentLODLevel->GetModules();
	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		UParticleModule* Module = CurrentLODLevel->ResolveModule(ModuleIndex, SpriteTemplate);
		if (IsBeamOnlyModule(Module) && !bCurrentTypeDataIsBeam)
		{
			continue;
		}
		if (Module && Module->IsEnabled() && Module->IsUpdateModule())
		{
			Module->Update(this, PayloadOffset, DeltaTime);
		}
	}
}
