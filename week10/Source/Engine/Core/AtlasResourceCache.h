#pragma once

#include "Asset/FontAtlasLoader.h"
#include "Asset/ParticleAtlasLoader.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"

struct ID3D11Device;

class FAtlasResourceCache
{
public:
	bool LoadGPUResources(ID3D11Device* Device);

	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;
	void RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows);

	FParticleResource* FindParticle(const FName& ParticleName);
	const FParticleResource* FindParticle(const FName& ParticleName) const;
	void RegisterParticle(const FName& ParticleName, const FString& InPath, uint32 Columns, uint32 Rows);

	void Clear();
	void Release();

private:
	FFontAtlasLoader FontLoader;
	FParticleAtlasLoader ParticleLoader;

	TMap<FString, FFontResource> FontResources;
	TMap<FString, FParticleResource> ParticleResources;
};
