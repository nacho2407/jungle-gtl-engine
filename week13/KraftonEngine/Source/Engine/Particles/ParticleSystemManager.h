#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/GarbageCollection.h"
#include "Asset/AssetRegistry.h"

class UParticleSystem;

class FParticleSystemManager : public TSingleton<FParticleSystemManager>
, public FGCObject
{
    friend class TSingleton<FParticleSystemManager>;

public:
    UParticleSystem* Load(const FString& Path);
    UParticleSystem* Find(const FString& Path) const;

    bool Save(UParticleSystem* Asset);

    void RefreshAvailableParticleSystems();

    const TArray<FAssetListItem>& GetAvailableParticleSystemFiles() const
    {
        return AvailableParticleSystemFiles;
    }


    void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
    TMap<FString, UParticleSystem*> LoadedParticleSystems;
    TArray<FAssetListItem>          AvailableParticleSystemFiles;
};
