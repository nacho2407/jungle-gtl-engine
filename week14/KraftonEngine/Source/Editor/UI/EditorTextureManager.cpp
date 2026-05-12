#include "EditorTextureManager.h"

#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <filesystem>

ID3D11ShaderResourceView* FEditorTextureManager::LoadTextureFromDisk(const FString& Path)
{
	ID3D11ShaderResourceView* Texture = nullptr;
	std::filesystem::path TexturePath(FPaths::ToWide(Path));

	if (FAILED(DirectX::CreateWICTextureFromFile(Device, TexturePath.c_str(), nullptr, &Texture)))
	{
		return nullptr;
	}

	return Texture;
}

void FEditorTextureManager::ReleaseTextureMap(TMap<FString, ID3D11ShaderResourceView*>& TextureMap)
{
	for (auto& Entry : TextureMap)
	{
		if (Entry.second)
		{
			Entry.second->Release();
			Entry.second = nullptr;
		}
	}

	TextureMap.clear();
}

bool FEditorTextureManager::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;
	return Device != nullptr;
}

void FEditorTextureManager::Shutdown()
{
	ClearThumbnails();
	ReleaseTextureMap(PersistentIcons);
	Device = nullptr;
}

ID3D11ShaderResourceView* FEditorTextureManager::GetOrLoadIcon(const FString& Path)
{
	if (auto It = PersistentIcons.find(Path); It != PersistentIcons.end())
	{
		return It->second;
	}

	ID3D11ShaderResourceView* Texture = LoadTextureFromDisk(Path);
	if (!Texture)
	{
		return nullptr;
	}

	PersistentIcons[Path] = Texture;
	return Texture;
}

ID3D11ShaderResourceView* FEditorTextureManager::GetOrLoadThumbnail(const FString& Path)
{
	if (auto It = TransientThumbnails.find(Path); It != TransientThumbnails.end())
	{
		return It->second;
	}

	ID3D11ShaderResourceView* Texture = LoadTextureFromDisk(Path);
	if (!Texture)
	{
		return nullptr;
	}

	TransientThumbnails[Path] = Texture;
	return Texture;
}

void FEditorTextureManager::ClearThumbnails()
{
	ReleaseTextureMap(TransientThumbnails);
}
