#pragma once

#include "Asset/FontAtlasLoader.h"
#include "Asset/SubUVAtlasLoader.h"
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

	FSubUVResource* FindSubUV(const FName& SubUVName);
	const FSubUVResource* FindSubUV(const FName& SubUVName) const;
	void RegisterSubUV(const FName& SubUVName, const FString& InPath, uint32 Columns, uint32 Rows);

	void Clear();
	void Release();

private:
	FFontAtlasLoader FontLoader;
	FSubUVAtlasLoader SubUVLoader;

	TMap<FString, FFontResource> FontResources;
	TMap<FString, FSubUVResource> SubUVResources;
};
