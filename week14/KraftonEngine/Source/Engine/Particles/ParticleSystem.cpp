#include "ParticleSystem.h"

#include "Materials/MaterialManager.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Serialization/Archive.h"

namespace
{
	UParticleModuleRequired* CreateDefaultRequiredModule(float EmitterDuration, bool bLooping)
	{
		UParticleModuleRequired* RequiredModule = UObjectManager::Get().CreateObject<UParticleModuleRequired>();
		UMaterialInterface* Material = FMaterialManager::Get().GetOrCreateMaterialInterface("Content/Material/Editor/DefualtParticleSprite.mat");
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
		MeshTypeData->MeshPath = "None";
		LODLevel->SetTypeDataModule(MeshTypeData);

		Emitter->AddLODLevel(LODLevel);
		return Emitter;
	}
}

UParticleLODLevel::~UParticleLODLevel()
{
	delete TypeDataModule;
	TypeDataModule = nullptr;

	for (UParticleModule* Module : Modules)
	{
		delete Module;
	}
	Modules.clear();
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	SerializeProperties(Ar, PF_Save);
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

	AddEmitter(CreateSpriteEmitter());
	AddEmitter(CreateMeshEmitter());
}

void UParticleSystem::AddEmitter(UParticleEmitter* Emitter)
{
	if (!Emitter)
	{
		return;
	}

	Emitters.push_back(Emitter);
}

void UParticleSystem::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);

	Ar << AssetPathFileName;

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
}
