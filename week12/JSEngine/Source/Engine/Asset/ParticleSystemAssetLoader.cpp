#include "Asset/ParticleSystemAssetLoader.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Object/Object.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleModules.h"
#include "Serialization/ObjectGraphSerializer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace
{
	bool IsLiveObject(const UObject* Object)
	{
		return Object != nullptr && UObjectManager::Get().ContainsObject(Object);
	}
}

// 경로를 정규화한 뒤 객체 그래프를 로드한 뒤 에셋을 불러옵니다. 로드된 객체 그래프의 live reference와 필수 모듈을 보정합니다.
UParticleSystem* FParticleSystemAssetLoader::Load(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || !FAssetPathPolicy::IsParticleSystemAssetPath(NormalizedPath))
	{
		return nullptr;
	}

	FObjectGraphSerializer Serializer;
	UObject* RootObject = Serializer.LoadFromFile(NormalizedPath, "UParticleSystem");
	UParticleSystem* ParticleSystem = Cast<UParticleSystem>(RootObject);
	if (!ParticleSystem)
	{
		UObjectManager::Get().DestroyObject(RootObject);
		return nullptr;
	}

	ParticleSystem->SetAssetPath(NormalizedPath);
	FixupParticleSystem(ParticleSystem);
	return ParticleSystem;
}

// 런타임 파티클 시스템을 객체 그래프 직렬화 → JSON 직렬화합니다.
bool FParticleSystemAssetLoader::Save(const FString& Path, const UParticleSystem* ParticleSystem) const
{
	if (!ParticleSystem)
	{
		return false;
	}

	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || !FAssetPathPolicy::IsParticleSystemAssetPath(NormalizedPath))
	{
		return false;
	}

	FixupParticleSystem(const_cast<UParticleSystem*>(ParticleSystem));

	FObjectGraphSerializer Serializer;
	return Serializer.SaveToFile(NormalizedPath, const_cast<UParticleSystem*>(ParticleSystem), "UParticleSystem");
}

// 확장자가 .particle인지 대소문자 구분 없이 검사합니다.
bool FParticleSystemAssetLoader::SupportsExtension(const FString& Extension) const
{
	FString LowerExtension = Extension;
	std::transform(
		LowerExtension.begin(),
		LowerExtension.end(),
		LowerExtension.begin(),
		[](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

	return LowerExtension == ".particle" || LowerExtension == "particle";
}


// 로드된 파티클 시스템의 live reference와 필수 모듈을 보정합니다.
void FParticleSystemAssetLoader::FixupParticleSystem(UParticleSystem* ParticleSystem) const
{
	if (!ParticleSystem)
	{
		return;
	}

	ParticleSystem->Emitters.erase(
		std::remove_if(
			ParticleSystem->Emitters.begin(),
			ParticleSystem->Emitters.end(),
			[](UParticleEmitter* Emitter)
			{
				return !IsLiveObject(Emitter);
			}),
		ParticleSystem->Emitters.end());

	for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
	{
		if (!IsLiveObject(Emitter))
		{
			continue;
		}

		Emitter->LODLevels.erase(
			std::remove_if(
				Emitter->LODLevels.begin(),
				Emitter->LODLevels.end(),
				[](UParticleLODLevel* LOD)
				{
					return !IsLiveObject(LOD);
				}),
			Emitter->LODLevels.end());

		for (int32 LODIndex = 0; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
		{
			UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
			if (!IsLiveObject(LOD))
			{
				continue;
			}

			LOD->Level = LODIndex;
			if (!IsLiveObject(LOD->RequiredModule))
			{
				LOD->RequiredModule = NewObject<UParticleModuleRequired>();
			}

			if (!IsLiveObject(LOD->SpawnModule))
			{
				LOD->SpawnModule = NewObject<UParticleModuleSpawn>();
			}

			if (!IsLiveObject(LOD->TypeDataModule))
			{
				LOD->TypeDataModule = NewObject<UParticleModuleTypeDataBase>();
			}

			LOD->Modules.erase(
				std::remove_if(
					LOD->Modules.begin(),
					LOD->Modules.end(),
					[LOD](UParticleModule* Module)
					{
						return !IsLiveObject(Module)
							|| Module == LOD->RequiredModule
							|| Module == LOD->SpawnModule
							|| Module == LOD->TypeDataModule;
					}),
				LOD->Modules.end());
		}

		Emitter->CacheEmitterModuleInfo();
	}
}
