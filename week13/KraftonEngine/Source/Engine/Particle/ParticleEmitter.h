#pragma once

#include "Object/Object.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Engine/Particle/ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModuleRequired;
class UParticleModuleTypeDataBase;
class FArchive;

UCLASS()
class UParticleEmitter : public UObject
{
public:
    GENERATED_BODY()

    UParticleEmitter()           = default;
    ~UParticleEmitter() override = default;

    void CacheEmitterModuleInfo();
    void Serialize(FArchive& Ar) override;

    TArray<UParticleLODLevel*>&       GetLODLevels() { return LODLevels; }
    const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }

    UParticleLODLevel* GetLODLevel(int32 LODIndex) const;

    bool IsEnabled() const { return bEnabled; }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

private:
    TArray<UParticleLODLevel*> LODLevels;

    int32 ParticleSize   = 0;
    int32 ParticleStride = 0;
    bool  bEnabled       = true;
};
