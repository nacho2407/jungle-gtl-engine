#pragma once
#include "Particle/ParticleAsset.h"

class FParticleSystemAssetLoader
{
public:
	UParticleSystem* Load(const FString& Path);
	bool Save(const FString& Path, const UParticleSystem* ParticleSystem);

private:
	// TODO: 개별 Serializer 삭제하고 리플렉션으로 통일
	bool SerializeParticleSystem(FArchive& Ar, UParticleSystem* ParticleSystem);
	bool SerializeEmitter(FArchive& Ar, UParticleEmitter* Emitter);
	bool SerializeLODLevel(FArchive& Ar, UParticleLODLevel* LODLevel);
	bool SerializeModule(FArchive& Ar, UParticleModule* Module);
};
