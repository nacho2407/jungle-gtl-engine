#include "ParticleSystem.h"

#include "ParticleEmitter.h"
#include "Object/GarbageCollection.h"
#include "Serialization/Archive.h"

void UParticleSystem::Serialize(FArchive& Ar)
{
    int32 Version = 0;
    Ar << Version;
    Ar << LODDistances;
    
    if (Ar.IsLoading())
    {
        Emitters.clear();
    }

    int32 EmitterCount = static_cast<int32>(Emitters.size());
    Ar << EmitterCount;

    if (Ar.IsSaving())
    {
        for (UParticleEmitter* Emitter : Emitters)
        {
            bool bValid = (Emitter != nullptr);
            Ar << bValid;
            if (bValid)
            {
                Emitter->Serialize(Ar);
            }
        }
    }
    else if (Ar.IsLoading())
    {
        for (int32 Index = 0; Index < EmitterCount; ++Index)
        {
            bool bValid = false;
            Ar << bValid;
            if (!bValid)
            {
                Emitters.push_back(nullptr);
                continue;
            }

            UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>(this);
            Emitter->Serialize(Ar);
            Emitters.push_back(Emitter);
        }
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
