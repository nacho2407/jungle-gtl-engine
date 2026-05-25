#include "Particles/ParticleLODLevel.h"

#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Event/ParticleModuleEventGenerator.h"

#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>

namespace
{
	// 모듈 한 슬롯을 (ClassName, [payload]) 형태로 직렬화한다. ClassName 으로 ObjectFactory
	// 디스패치를 해서 적절한 구체 클래스를 만들어준 뒤 해당 인스턴스의 Serialize 를 호출.
	// 로드 시 클래스 미등록/타입 mismatch면 슬롯은 nullptr로 두고 다음으로 넘어간다.
	template<class T>
	void SerializeOwnedModule(FArchive& Ar, T*& Module, UObject* Outer)
	{
		FString ClassName = (Ar.IsSaving() && Module)
			? FString(Module->GetClass()->GetName())
			: FString("None");
		Ar << ClassName;

		if (Ar.IsLoading())
		{
			Module = nullptr;
			if (!ClassName.empty() && ClassName != "None")
			{
				UObject* Created = FObjectFactory::Get().Create(ClassName, Outer);
				Module = Cast<T>(Created);
				if (!Module && Created)
				{
					UObjectManager::Get().DestroyObject(Created);
				}
			}
		}

		if (Module)
		{
			Module->Serialize(Ar);
		}
	}
}

int32 UParticleLODLevel::CalculateMaxActiveParticleCount() const
{
	int32 Estimate = 32;

	if (SpawnModule)
	{
		Estimate += static_cast<int32>(SpawnModule->SpawnRate * 2.0f);
		Estimate += SpawnModule->GetMaximumBurstCount();
	}

	if (RequiredModule)
	{
		const float Duration = std::max(0.001f, RequiredModule->EmitterDuration);
		if (SpawnModule)
		{
			Estimate += static_cast<int32>(SpawnModule->SpawnRate * SpawnModule->SpawnRateScale * Duration);
		}
	}

	return std::max(Estimate, 32);
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.clear();
	UpdateModules.clear();
	OrbitModules.clear();

	if (SpawnModule)
	{
		SpawnModules.push_back(SpawnModule);
	}

	// 각 모듈이 자신의 bSpawnModule/bUpdateModule 플래그로 어느 단계에 들어갈지 결정한다.
	// 같은 모듈이 두 리스트에 모두 들어갈 수 있다 (예: Velocity, Size, Color).
	for (UParticleModule* Module : Modules)
	{
		if (!Module)
		{
			continue;
		}

		// UE Cascade keeps Beam/Trail special modules out of the generic sprite
		// spawn/update loops. Their TypeData emitter instances call them in the
		// source/target/noise/modifier or trail-source order.
		if (Module->GetModuleType() == EPMT_Beam || Module->GetModuleType() == EPMT_Trail)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.push_back(Module);
		}
		if (Module->bUpdateModule)
		{
			UpdateModules.push_back(Module);
		}

		if (Module->IsA<UParticleModuleTypeDataBase>())
		{
			TypeDataModule = Cast<UParticleModuleTypeDataBase>(Module);
		}
		else if (Module->IsA<UParticleModuleEventGenerator>())
		{
			EventGenerator = Cast<UParticleModuleEventGenerator>(Module);
		}
	}
}

void UParticleLODLevel::Serialize(FArchive& Ar)
{
	int32 Version = 0;
	Ar << Version;

	Ar << Level;

	bool bSavedEnabled = bEnabled;
	Ar << bSavedEnabled;
	if (Ar.IsLoading()) bEnabled = bSavedEnabled;

	SerializeOwnedModule(Ar, RequiredModule, this);
	SerializeOwnedModule(Ar, SpawnModule, this);
	SerializeOwnedModule(Ar, TypeDataModule, this);

	uint32 ModuleCount = Ar.IsSaving() ? static_cast<uint32>(Modules.size()) : 0;
	Ar << ModuleCount;

	if (Ar.IsLoading())
	{
		Modules.clear();
		Modules.resize(ModuleCount, nullptr);
	}

	for (uint32 i = 0; i < ModuleCount; ++i)
	{
		UParticleModule* M = Ar.IsSaving() ? Modules[i] : nullptr;
		SerializeOwnedModule(Ar, M, this);
		if (Ar.IsLoading()) Modules[i] = M;
	}

	if (Ar.IsLoading())
	{
		UpdateModuleLists();
	}
}
