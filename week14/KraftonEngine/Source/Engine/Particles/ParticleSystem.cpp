#include "ParticleSystem.h"

#include "Materials/MaterialManager.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryArchive.h"

namespace
{
	UParticleModuleRequired* CreateDefaultRequiredModule(float EmitterDuration, bool bLooping)
	{
		UParticleModuleRequired* RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();
		UMaterialInterface* Material = FMaterialManager::Get().GetOrCreateMaterialInterface("Content/Material/Editor/DefaultParticleSprite.mat");
		RequiredModule->Material = Material;
		RequiredModule->MaterialPath = "Content/Material/Editor/DefaultParticleSprite.mat";
		RequiredModule->EmitterDuration = EmitterDuration;
		RequiredModule->bLooping = bLooping;
		return RequiredModule;
	}

	UParticleLODLevel* CreateDefaultLODLevel(float EmitterDuration, bool bLooping, float SpawnRate, float Lifetime, const FVector& StartLocation, const FVector& StartVelocity, const FVector& StartSize, const FVector4& StartColor, const FVector4& EndColor)
	{
		UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		LODLevel->SetLevel(0);
		LODLevel->SetEnabled(true);

		UParticleModuleSpawn* SpawnModule = UObjectManager::Get().CreateObject<UParticleModuleSpawn>();
		SpawnModule->SpawnRate = SpawnRate;

		UParticleModuleLifetime* LifetimeModule = UObjectManager::Get().CreateObject<UParticleModuleLifetime>();
		LifetimeModule->Lifetime = Lifetime;

		UParticleModuleLocation* LocationModule = UObjectManager::Get().CreateObject<UParticleModuleLocation>();
		LocationModule->StartLocation = StartLocation;

		UParticleModuleVelocity* VelocityModule = UObjectManager::Get().CreateObject<UParticleModuleVelocity>();
		VelocityModule->StartVelocity = StartVelocity;

		UParticleModuleColor* ColorModule = UObjectManager::Get().CreateObject<UParticleModuleColor>();
		ColorModule->StartColor = StartColor;
		ColorModule->EndColor = EndColor;

		UParticleModuleSize* SizeModule = UObjectManager::Get().CreateObject<UParticleModuleSize>();
		SizeModule->StartSize = StartSize;

		LODLevel->GetMutableModules().push_back(CreateDefaultRequiredModule(EmitterDuration, bLooping));
		LODLevel->GetMutableModules().push_back(SpawnModule);
		LODLevel->GetMutableModules().push_back(LifetimeModule);
		LODLevel->GetMutableModules().push_back(LocationModule);
		LODLevel->GetMutableModules().push_back(VelocityModule);
		LODLevel->GetMutableModules().push_back(ColorModule);
		LODLevel->GetMutableModules().push_back(SizeModule);
		LODLevel->SetAllModuleEditStates(EParticleModuleEditState::Duplicated);

		return LODLevel;
	}

	UParticleEmitter* CreateSpriteEmitter()
	{
		UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
		Emitter->SetMaxActiveParticles(100);
		Emitter->SetEmitterDuration(1.0f);
		Emitter->SetLooping(true);
		Emitter->AddLODLevel(CreateDefaultLODLevel(
			Emitter->GetEmitterDuration(),
			Emitter->IsLooping(),
			18.0f,
			1.0f,
			FVector(-24.0f, 0.0f, 0.0f),
			FVector(0.0f, 90.0f, 20.0f),
			FVector(10.0f, 10.0f, 1.0f),
			FVector4(1.0f, 0.72f, 0.28f, 1.0f),
			FVector4(0.35f, 0.35f, 0.35f, 0.0f)));
		return Emitter;
	}

	UParticleEmitter* CreateMeshEmitter()
	{
		UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
		Emitter->SetMaxActiveParticles(48);
		Emitter->SetEmitterDuration(1.6f);
		Emitter->SetLooping(false);

		UParticleLODLevel* LODLevel = CreateDefaultLODLevel(
			Emitter->GetEmitterDuration(),
			Emitter->IsLooping(),
			7.0f,
			1.6f,
			FVector(28.0f, 0.0f, 12.0f),
			FVector(0.0f, 55.0f, 80.0f),
			FVector(4.0f, 4.0f, 4.0f),
			FVector4(0.55f, 0.78f, 1.0f, 1.0f),
			FVector4(0.10f, 0.16f, 0.24f, 0.0f));

		UParticleModuleTypeDataMesh* MeshTypeData = UObjectManager::Get().CreateObject<UParticleModuleTypeDataMesh>();
		MeshTypeData->MeshPath = "Content/Data/BasicShape/Cylinder_StaticMesh.uasset";
		LODLevel->SetTypeDataModule(MeshTypeData);

		Emitter->AddLODLevel(LODLevel);
		return Emitter;
	}

	UParticleLODLevel* DuplicateLODLevel(UParticleLODLevel* SourceLODLevel)
	{
		if (!SourceLODLevel)
		{
			return nullptr;
		}

		FMemoryArchive Writer(/*bInIsSaving=*/true);
		SourceLODLevel->Serialize(Writer);

		UParticleLODLevel* DuplicatedLODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		if (!DuplicatedLODLevel)
		{
			return nullptr;
		}

		const FName UniqueName = DuplicatedLODLevel->GetFName();
		FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
		DuplicatedLODLevel->Serialize(Reader);
		DuplicatedLODLevel->SetFName(UniqueName);
		DuplicatedLODLevel->GetMutableModuleEditStates() = SourceLODLevel->GetModuleEditStates();
		DuplicatedLODLevel->NormalizeModuleEditStates(EParticleModuleEditState::InheritedLocked);
		return DuplicatedLODLevel;
	}
}

UParticleLODLevel::~UParticleLODLevel()
{
	delete TypeDataModule;
	TypeDataModule = nullptr;

	for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
	{
		if (GetModuleEditState(ModuleIndex) == EParticleModuleEditState::Shared)
		{
			continue;
		}
		delete Modules[ModuleIndex];
	}
	Modules.clear();
	ModuleEditStates.clear();
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	SerializeProperties(Ar, PF_Save);
	NormalizeModuleEditStates(Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
}

EParticleModuleEditState UParticleLODLevel::GetModuleEditState(int32 ModuleIndex) const
{
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(ModuleEditStates.size()))
	{
		return Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked;
	}

	return ModuleEditStates[ModuleIndex];
}

void UParticleLODLevel::SetModuleEditState(int32 ModuleIndex, EParticleModuleEditState State)
{
	NormalizeModuleEditStates(Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(ModuleEditStates.size()))
	{
		return;
	}

	ModuleEditStates[ModuleIndex] = State;
}

void UParticleLODLevel::NormalizeModuleEditStates(EParticleModuleEditState DefaultState)
{
	const int32 ModuleCount = static_cast<int32>(Modules.size());
	if (static_cast<int32>(ModuleEditStates.size()) > ModuleCount)
	{
		ModuleEditStates.resize(ModuleCount);
	}
	else if (static_cast<int32>(ModuleEditStates.size()) < ModuleCount)
	{
		ModuleEditStates.resize(ModuleCount, DefaultState);
	}
}

void UParticleLODLevel::SetAllModuleEditStates(EParticleModuleEditState State)
{
	ModuleEditStates.assign(Modules.size(), State);
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		delete LODLevel;
	}
	LODLevels.clear();
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* Component)
{
	UParticleLODLevel* LODLevel = GetLODLevel(0);
	if (LODLevel && LODLevel->GetTypeDataModule())
	{
		if (FParticleEmitterInstance* Instance = LODLevel->GetTypeDataModule()->CreateInstance(this, Component))
		{
			return Instance;
		}
	}

	FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
	Instance->Init(this, Component);
	return Instance;
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(LODLevels.size()))
	{
		return nullptr;
	}

	return LODLevels[Index];
}

void UParticleEmitter::AddLODLevel(UParticleLODLevel* LODLevel)
{
	if (!LODLevel)
	{
		return;
	}

	LODLevels.push_back(LODLevel);
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << MaxActiveParticles;
	Ar << EmitterDuration;
	Ar << bLooping;

	uint32 LODLevelCount = Ar.IsSaving() ? static_cast<uint32>(LODLevels.size()) : 0;
	Ar << LODLevelCount;

	if (Ar.IsLoading())
	{
		for (UParticleLODLevel* LODLevel : LODLevels)
		{
			delete LODLevel;
		}
		LODLevels.clear();
		LODLevels.reserve(LODLevelCount);
	}

	for (uint32 Index = 0; Index < LODLevelCount; ++Index)
	{
		UParticleLODLevel* LODLevel = Ar.IsSaving() ? LODLevels[Index] : new UParticleLODLevel();
		LODLevel->Serialize(Ar);

		if (Ar.IsLoading())
		{
			LODLevels.push_back(LODLevel);
		}
	}
}

UParticleSystem::~UParticleSystem()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		delete Emitter;
	}
	Emitters.clear();
}

void UParticleSystem::InitializeDefaultEmitters()
{
	if (!Emitters.empty())
	{
		return;
	}

	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	AddEmitter(CreateSpriteEmitter());
	AddEmitter(CreateMeshEmitter());
	NormalizeLODLevels();
}

void UParticleSystem::AddEmitter(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return;
	}

	NormalizeEmitterLODLevels(Emitter);
	Emitters.push_back(Emitter);
}

UParticleEmitter* UParticleSystem::AddDefaultEmitter()
{
	UParticleEmitter* Emitter = CreateSpriteEmitter();
	AddEmitter(Emitter);
	return Emitter;
}

float UParticleSystem::GetLODDistance(int32 Index) const
{
	if (Index < 0 || Index >= static_cast<int32>(LODDistances.size()))
	{
		return 0.0f;
	}

	return LODDistances[Index];
}

int32 UParticleSystem::SelectLODLevelIndex(float Distance) const
{
	if (LODDistances.empty())
	{
		return 0;
	}

	int32 BestIndex = 0;
	float BestDistance = -1.0f;
	for (int32 Index = 0; Index < static_cast<int32>(LODDistances.size()); ++Index)
	{
		const float LODDistance = LODDistances[Index];
		if (Distance >= LODDistance && LODDistance >= BestDistance)
		{
			BestIndex = Index;
			BestDistance = LODDistance;
		}
	}

	return BestIndex;
}

void UParticleSystem::NormalizeLODLevels()
{
	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	LODDistances[0] = 0.0f;
	for (int32 Index = 1; Index < static_cast<int32>(LODDistances.size()); ++Index)
	{
		if (LODDistances[Index] <= LODDistances[Index - 1])
		{
			LODDistances[Index] = LODDistances[Index - 1] + 1000.0f;
		}
	}

	for (UParticleEmitter* Emitter : Emitters)
	{
		NormalizeEmitterLODLevels(Emitter);
	}
}

void UParticleSystem::NormalizeEmitterLODLevels(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return;
	}

	if (LODDistances.empty())
	{
		LODDistances.push_back(0.0f);
	}

	TArray<UParticleLODLevel*>& LODLevels = Emitter->GetMutableLODLevels();
	while (LODLevels.empty())
	{
		LODLevels.push_back(CreateDefaultLODLevel(
			Emitter->GetEmitterDuration(),
			Emitter->IsLooping(),
			10.0f,
			1.0f,
			FVector::ZeroVector,
			FVector(0.0f, 100.0f, 0.0f),
			FVector(10.0f, 10.0f, 1.0f),
			FVector4(1.0f, 1.0f, 1.0f, 1.0f),
			FVector4(1.0f, 1.0f, 1.0f, 0.0f)));
	}

	while (static_cast<int32>(LODLevels.size()) < static_cast<int32>(LODDistances.size()))
	{
		UParticleLODLevel* SourceLODLevel = LODLevels.back();
		UParticleLODLevel* NewLODLevel = DuplicateLODLevel(SourceLODLevel);
		if (!NewLODLevel)
		{
			break;
		}
		NewLODLevel->SetLevel(static_cast<int32>(LODLevels.size()));
		NewLODLevel->SetAllModuleEditStates(EParticleModuleEditState::InheritedLocked);
		LODLevels.push_back(NewLODLevel);
	}

	while (static_cast<int32>(LODLevels.size()) > static_cast<int32>(LODDistances.size()))
	{
		UParticleLODLevel* RemovedLODLevel = LODLevels.back();
		LODLevels.pop_back();
		delete RemovedLODLevel;
	}

	for (int32 Index = 0; Index < static_cast<int32>(LODLevels.size()); ++Index)
	{
		if (LODLevels[Index])
		{
			LODLevels[Index]->SetLevel(Index);
			LODLevels[Index]->NormalizeModuleEditStates(Index == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		}
	}
}

void UParticleSystem::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << AssetPathFileName;
	Ar << LODDistances;

	uint32 EmitterCount = Ar.IsSaving() ? static_cast<uint32>(Emitters.size()) : 0;
	Ar << EmitterCount;

	if (Ar.IsLoading())
	{
		for (UParticleEmitter* Emitter : Emitters)
		{
			delete Emitter;
		}
		Emitters.clear();
		Emitters.reserve(EmitterCount);
	}

	for (uint32 Index = 0; Index < EmitterCount; ++Index)
	{
		UParticleEmitter* Emitter = Ar.IsSaving() ? Emitters[Index] : new UParticleEmitter();
		Emitter->Serialize(Ar);

		if (Ar.IsLoading())
		{
			Emitters.push_back(Emitter);
		}
	}

	if (Ar.IsLoading())
	{
		NormalizeLODLevels();
	}
}
