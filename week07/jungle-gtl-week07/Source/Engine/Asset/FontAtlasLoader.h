#pragma once

#include "Asset/IAssetLoader.h"
#include "Core/ResourceTypes.h"

struct ID3D11Device;

class FFontAtlasLoader : public IAssetLoader
{
public:
	bool Load(const FName& FontName, const FString& Path, uint32 Columns, uint32 Rows,
		ID3D11Device* Device, FFontResource& OutResource) const;

	bool SupportsExtension(const FString& Extension) const override;
	FString GetLoaderName() const override;
};
