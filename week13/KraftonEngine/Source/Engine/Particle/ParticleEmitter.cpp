#include "ParticleEmitter.h"

void UParticleEmitter::CacheEmitterModuleInfo()
{
    // TODO: LODLevel/ParticleModule 들어오면 작업
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
    if (LODIndex < 0 || LODIndex >= static_cast<int32>(LODLevels.size()))
    {
        return nullptr;
    }

    return LODLevels[LODIndex];
}
