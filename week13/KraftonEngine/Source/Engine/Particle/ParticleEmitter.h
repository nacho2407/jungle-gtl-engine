#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Engine/Particle/ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModuleRequired;
class UParticleModuleTypeDataBase;

UCLASS()
class UParticleEmitter : public UObject
{
public:
    GENERATED_BODY()

    UParticleEmitter()           = default;
    ~UParticleEmitter() override = default;

    void CacheEmitterModuleInfo();

    TArray<UParticleLODLevel*>&       GetLODLevels() { return LODLevels; }
    const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }

    UParticleLODLevel* GetLODLevel(int32 LODIndex) const;

private:
    TArray<UParticleLODLevel*> LODLevels;

    int32 ParticleSize   = 0;
    int32 ParticleStride = 0;
};
