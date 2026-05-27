#include "ParticleSystem.h"

#include "Materials/MaterialManager.h"
#include "Particles/Module/ParticleModule.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryArchive.h"

#include "Object/ReferenceCollector.h"

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

	UParticleLODLevel* CreateDefaultLODLevel(float EmitterDuration, bool bLooping, float SpawnRate, float Lifetime, const FVector& StartVelocity, const FVector& StartSize, const FVector4& StartColor, const FVector4& EndColor)
	{
		UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		LODLevel->SetLevel(0);
		LODLevel->SetEnabled(true);

		UParticleModuleSpawn* SpawnModule = UObjectManager::Get().CreateObject<UParticleModuleSpawn>();
		SpawnModule->SpawnRate.Mode = EDistributionValueMode::Constant;
		SpawnModule->SpawnRate.Constant = SpawnRate;
		SpawnModule->SpawnRate.MinValue = SpawnRate;
		SpawnModule->SpawnRate.MaxValue = SpawnRate;

		UParticleModuleLifetime* LifetimeModule = UObjectManager::Get().CreateObject<UParticleModuleLifetime>();
		LifetimeModule->Lifetime.Mode = EDistributionValueMode::Uniform;
		LifetimeModule->Lifetime.Constant = Lifetime;
		LifetimeModule->Lifetime.MinValue = Lifetime;
		LifetimeModule->Lifetime.MaxValue = Lifetime;

		UParticleModuleVelocity* VelocityModule = UObjectManager::Get().CreateObject<UParticleModuleVelocity>();
		VelocityModule->StartVelocity.Mode = EDistributionValueMode::Uniform;
		VelocityModule->StartVelocity.Constant = StartVelocity;
		VelocityModule->StartVelocity.MinValue = FVector(-10.0f, -10.0f, 50.0f);
		VelocityModule->StartVelocity.MaxValue = FVector(10.0f, 10.0f, 100.0f);

		UParticleModuleColor* ColorModule = UObjectManager::Get().CreateObject<UParticleModuleColor>();
		ColorModule->StartColor.Mode = EDistributionValueMode::Constant;
		ColorModule->StartColor.Constant = FVector(StartColor.X, StartColor.Y, StartColor.Z);
		ColorModule->StartColor.MinValue = ColorModule->StartColor.Constant;
		ColorModule->StartColor.MaxValue = ColorModule->StartColor.Constant;
		ColorModule->StartAlpha.Mode = EDistributionValueMode::Constant;
		ColorModule->StartAlpha.Constant = StartColor.W;
		ColorModule->StartAlpha.MinValue = StartColor.W;
		ColorModule->StartAlpha.MaxValue = StartColor.W;
		ColorModule->EndColor.Mode = EDistributionValueMode::Constant;
		ColorModule->EndColor.Constant = FVector(EndColor.X, EndColor.Y, EndColor.Z);
		ColorModule->EndColor.MinValue = ColorModule->EndColor.Constant;
		ColorModule->EndColor.MaxValue = ColorModule->EndColor.Constant;
		ColorModule->EndAlpha.Mode = EDistributionValueMode::Constant;
		ColorModule->EndAlpha.Constant = EndColor.W;
		ColorModule->EndAlpha.MinValue = EndColor.W;
		ColorModule->EndAlpha.MaxValue = EndColor.W;

		UParticleModuleSize* SizeModule = UObjectManager::Get().CreateObject<UParticleModuleSize>();
		SizeModule->StartSize.Mode = EDistributionValueMode::Uniform;
		SizeModule->StartSize.Constant = StartSize;
		SizeModule->StartSize.MinValue = StartSize;
		SizeModule->StartSize.MaxValue = StartSize;

		LODLevel->GetMutableModules().push_back(CreateDefaultRequiredModule(EmitterDuration, bLooping));
		LODLevel->GetMutableModules().push_back(SpawnModule);
		LODLevel->GetMutableModules().push_back(LifetimeModule);
		LODLevel->GetMutableModules().push_back(SizeModule);
		LODLevel->GetMutableModules().push_back(VelocityModule);
		LODLevel->GetMutableModules().push_back(ColorModule);
		LODLevel->SetAllModuleEditStates(EParticleModuleEditState::Duplicated);
		LODLevel->SetTypeDataEditState(EParticleModuleEditState::Duplicated);

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
			20.0f,
			1.0f,
			FVector(0.0f, 0.0f, 0.0f),
			FVector(25.0f, 25.0f, 25.0f),
			FVector4(1.0f, 1.0f, 1.0f, 1.0f),
			FVector4(1.0f, 1.0f, 1.0f, 0.0f)));
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
		DuplicatedLODLevel->SetTypeDataEditState(SourceLODLevel->GetTypeDataEditState());
		DuplicatedLODLevel->NormalizeModuleEditStates(EParticleModuleEditState::InheritedLocked);
		return DuplicatedLODLevel;
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
	ModuleEditStates.clear();
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
	SerializeProperties(Ar, PF_Save);
	Ar << ModuleEditStates;
	int32 TypeDataState = static_cast<int32>(TypeDataEditState);
	Ar << TypeDataState;
	if (Ar.IsLoading())
	{
		if (TypeDataState < static_cast<int32>(EParticleModuleEditState::InheritedLocked) ||
			TypeDataState > static_cast<int32>(EParticleModuleEditState::Shared))
		{
			TypeDataState = static_cast<int32>(Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
		}
		TypeDataEditState = static_cast<EParticleModuleEditState>(TypeDataState);
	}
	NormalizeModuleEditStates(Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked);
}

EParticleModuleEditState UParticleLODLevel::GetTypeDataEditState() const
{
	return Level == 0 ? EParticleModuleEditState::Duplicated : TypeDataEditState;
}

EParticleModuleEditState UParticleLODLevel::GetModuleEditState(int32 ModuleIndex) const
{
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(ModuleEditStates.size()))
	{
		return Level == 0 ? EParticleModuleEditState::Duplicated : EParticleModuleEditState::InheritedLocked;
	}

	return ModuleEditStates[ModuleIndex];
}

UParticleModuleTypeDataBase* UParticleLODLevel::ResolveTypeDataModule(const UParticleEmitter* OwnerEmitter) const
{
	if (GetTypeDataEditState() != EParticleModuleEditState::Shared || !OwnerEmitter)
	{
		return TypeDataModule;
	}

	for (int32 SourceLODIndex = Level - 1; SourceLODIndex >= 0; --SourceLODIndex)
	{
		const UParticleLODLevel* SourceLODLevel = OwnerEmitter->GetLODLevel(SourceLODIndex);
		if (!SourceLODLevel || SourceLODLevel == this)
		{
			continue;
		}

		if (UParticleModuleTypeDataBase* SourceTypeData = SourceLODLevel->ResolveTypeDataModule(OwnerEmitter))
		{
			return SourceTypeData;
		}
	}

	return TypeDataModule;
}

UParticleModule* UParticleLODLevel::ResolveModule(int32 ModuleIndex, const UParticleEmitter* OwnerEmitter) const
{
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(Modules.size()))
	{
		return nullptr;
	}

	if (GetModuleEditState(ModuleIndex) != EParticleModuleEditState::Shared || !OwnerEmitter)
	{
		return Modules[ModuleIndex];
	}

	for (int32 SourceLODIndex = Level - 1; SourceLODIndex >= 0; --SourceLODIndex)
	{
		const UParticleLODLevel* SourceLODLevel = OwnerEmitter->GetLODLevel(SourceLODIndex);
		if (!SourceLODLevel || SourceLODLevel == this)
		{
			continue;
		}

		UParticleModule* SourceModule = SourceLODLevel->ResolveModule(ModuleIndex, OwnerEmitter);
		if (SourceModule)
		{
			return SourceModule;
		}
	}

	return Modules[ModuleIndex];
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
	UParticleModuleTypeDataBase* TypeDataModule = LODLevel ? LODLevel->ResolveTypeDataModule(this) : nullptr;
	if (TypeDataModule)
	{
		if (FParticleEmitterInstance* Instance = TypeDataModule->CreateInstance(this, Component))
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
			20.0f,
			1.0f,
			FVector(0.0f, 0.0f, 0.0f),
			FVector(25.0f, 25.0f, 25.0f),
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
		NewLODLevel->SetTypeDataEditState(EParticleModuleEditState::InheritedLocked);
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

void UParticleSystem::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	for (UParticleEmitter* Emitter : Emitters)
	{
		Collector.AddReferencedObject(Emitter);
	}
}

void UParticleEmitter::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		Collector.AddReferencedObject(LODLevel);
	}
}

void UParticleLODLevel::AddReferencedObjects(FReferenceCollector& Collector)
{
	UObject::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(TypeDataModule);

	for (UParticleModule* Module : Modules)
	{
		Collector.AddReferencedObject(Module);
	}
}