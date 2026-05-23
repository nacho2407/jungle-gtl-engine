#pragma once

class FParticleSystemAssetLoader
{
public:
	UParticleSystem* Load(const FString& Path);
	bool Save(const FString& Path, const UParticleSystem* ParticleSystem);

private:
	bool SerializeParticleSystem(FArchive& Ar, UParticleSystem* ParticleSystem);
	bool SerializeEmitter(FArchive& Ar, UParticleEmitter* Emitter);
	bool SerializeLODLevel(FArchive& Ar, UParticleLODLevel* LODLevel);
	bool SerializeModule(FArchive& Ar, UParticleModule* Module);
}
