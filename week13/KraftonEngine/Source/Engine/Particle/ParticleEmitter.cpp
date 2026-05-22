#include "ParticleEmitter.h"

#include "Serialization/Archive.h"

void UParticleEmitter::CacheEmitterModuleInfo()
{
    // TODO: LODLevel/ParticleModule 들어오면 작업
}

void UParticleEmitter::Serialize(FArchive& Ar)
{
    int32 Version = 0;
    Ar << Version;
    Ar << bEnabled;
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
    if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevels.size()))
    {
        return nullptr;
    }

    return LODLevels[LODIndex];
}
