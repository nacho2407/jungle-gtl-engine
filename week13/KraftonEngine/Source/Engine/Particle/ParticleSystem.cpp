#include "ParticleSystem.h"
#include "Serialization/Archive.h"

void UParticleSystem::Serialize(FArchive& Ar)
{
    int32 Version = 0;
    Ar << Version;

    if (Ar.IsLoading())
    {
        Emitters.clear();
    }
}
