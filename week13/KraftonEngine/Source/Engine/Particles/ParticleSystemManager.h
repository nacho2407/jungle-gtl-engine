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


    const char* GetReferencerName() const override { return "FParticleSystemManager"; }
    void AddReferencedObjects(FReferenceCollector& Collector) override;
    void ClearCache();
private:
    TMap<FString, UParticleSystem*> LoadedParticleSystems;
    TArray<FAssetListItem>          AvailableParticleSystemFiles;
};
