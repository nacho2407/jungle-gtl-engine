#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

class UParticleSystem;
class FReferenceCollector;

class FParticleSystemManager : public TSingleton<FParticleSystemManager>
{
	friend class TSingleton<FParticleSystemManager>;

public:
	UParticleSystem* Load(const FString& Path);
	UParticleSystem* Find(const FString& Path) const;
	void Register(const FString& Path, UParticleSystem* System);

	bool Save(UParticleSystem* System);
	void RefreshAvailableParticleSystems();
	const TArray<FAssetListItem>& GetAvailableParticleSystemFiles() const { return AvailableParticleSystemFiles; }

	void Release();

	// GC
	void AddReferencedObjects(FReferenceCollector& Collector);
private:
	FParticleSystemManager() = default;

	TMap<FString, UParticleSystem*> LoadedParticleSystems;
	TArray<FAssetListItem> AvailableParticleSystemFiles;
};
