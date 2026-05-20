#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include <d3d11.h>

struct ID3D11ShaderResourceView;

class FEditorTextureManager : public TSingleton<FEditorTextureManager>
{
public:
	bool Initialize(ID3D11Device* InDevice);
	void Shutdown();

	ID3D11ShaderResourceView* GetOrLoadIcon(const FString& Path);
	ID3D11ShaderResourceView* GetOrLoadThumbnail(const FString& Path);
	void ClearThumbnails();

private:
	ID3D11ShaderResourceView* LoadTextureFromDisk(const FString& Path);
	void ReleaseTextureMap(TMap<FString, ID3D11ShaderResourceView*>& TextureMap);

private:
	ID3D11Device* Device = nullptr;
	TMap<FString, ID3D11ShaderResourceView*> PersistentIcons;
	TMap<FString, ID3D11ShaderResourceView*> TransientThumbnails;
};
