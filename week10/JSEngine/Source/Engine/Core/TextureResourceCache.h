#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/Texture.h"

#include <d3d11.h>

class FTextureResourceCache
{
public:
	ID3D11ShaderResourceView* GetDefaultWhiteSRV() const;
	UTexture* Get(const FString& Path) const;
	UTexture* Load(const FString& Path, ID3D11Device* Device);
	void Register(const FString& Key, UTexture* Texture, uint64 WriteTimeTicks = 0);
	bool Contains(const FString& Key) const;
	void Release();

private:
	uint64 GetFileWriteTimeTicks(const FString& Path) const;

private:
	TMap<FString, UTexture*> Textures;
	TMap<FString, uint64> TextureWriteTimeTicks;
};
