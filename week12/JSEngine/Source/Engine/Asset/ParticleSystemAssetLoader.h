#pragma once

#include "Core/Containers/String.h"

class UParticleSystem;

class FParticleSystemAssetLoader
{
public:
	bool Save(const FString& Path, const UParticleSystem* ParticleSystem) const;
	UParticleSystem* Load(const FString& Path) const;
	bool SupportsExtension(const FString& Extension) const;

private:
	void FixupParticleSystem(UParticleSystem* ParticleSystem, int32 AssetVersion) const;
};
