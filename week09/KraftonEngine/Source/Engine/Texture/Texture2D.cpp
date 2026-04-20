#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "UI/EditorConsolePanel.h"
#include "Platform/Paths.h"
#include "WICTextureLoader.h"

#include <d3d11.h>
#include <filesystem>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, FTextureCacheEntry> UTexture2D::TextureCache;
std::vector<UTexture2D*> UTexture2D::RetiredTextures;

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}

		SRV->Release();
		SRV = nullptr;
	}

	auto It = TextureCache.find(SourceFilePath);
	if (It != TextureCache.end() && It->second.Texture == this)
	{
		TextureCache.erase(It);
	}
}

void UTexture2D::ReleaseAllGPU()
{
	for (auto& [Path, Entry] : TextureCache)
	{
		UTexture2D* Texture = Entry.Texture;
		if (Texture && Texture->SRV)
		{
			if (Texture->TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
				Texture->TrackedTextureMemory = 0;
			}
			Texture->SRV->Release();
			Texture->SRV = nullptr;
		}
	}
	TextureCache.clear();

    for (UTexture2D* Texture : RetiredTextures)
    {
        if (Texture)
        {
            if (Texture->SRV)
            {
                if (Texture->TrackedTextureMemory > 0)
                {
                    MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
                    Texture->TrackedTextureMemory = 0;
                }
                Texture->SRV->Release();
                Texture->SRV = nullptr;
            }
            UObjectManager::Get().DestroyObject(Texture);
        }
    }
    RetiredTextures.clear();
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device)
{
	if (FilePath.empty() || !Device) return nullptr;

    const std::filesystem::path FullPath = ResolveFullPath(FilePath);
    const FString CacheKey = FPaths::FromPath(FullPath);

	auto It = TextureCache.find(CacheKey);
	if (It != TextureCache.end())
	{
        if (!HasCacheEntryChanged(It->second))
        {
		    return It->second.Texture;
        }

        if (It->second.Texture)
        {
            RetiredTextures.push_back(It->second.Texture);
        }
        TextureCache.erase(It);
	}

	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(CacheKey, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	TextureCache[CacheKey] = BuildCacheEntry(FullPath);
    TextureCache[CacheKey].Texture = Texture;
	return Texture;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device)
{
	std::wstring WidePath = FPaths::ToWide(FilePath);

	ID3D11Resource* Resource = nullptr;
	HRESULT hr = DirectX::CreateWICTextureFromFileEx(
		Device, WidePath.c_str(),
		0,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0,
		0,
		DirectX::WIC_LOADER_IGNORE_SRGB,
		&Resource, &SRV);

	if (FAILED(hr))
	{
		UE_LOG("Failed to load texture: %s", FilePath.c_str());
		return false;
	}

	if (Resource)
	{
		TrackedTextureMemory = MemoryStats::CalculateTextureMemory(Resource);

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			Width = Desc.Width;
			Height = Desc.Height;
			Tex2D->Release();
		}

		if (TrackedTextureMemory > 0)
		{
			MemoryStats::AddTextureMemory(TrackedTextureMemory);
		}
		Resource->Release();
	}

	SourceFilePath = FilePath;
	return true;
}

std::filesystem::path UTexture2D::ResolveFullPath(const FString& FilePath)
{
    std::filesystem::path Path = FPaths::ToPath(FPaths::ToWide(FilePath));
    if (!Path.is_absolute())
    {
        Path = FPaths::ToPath(FPaths::RootDir()) / Path;
    }

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
    if (!Ec)
    {
        return Canonical;
    }

    return Path.lexically_normal();
}

FTextureCacheEntry UTexture2D::BuildCacheEntry(const std::filesystem::path& FilePath)
{
    FTextureCacheEntry Entry;
    Entry.FullPath = FPaths::FromPath(FilePath);

    std::error_code Ec;
    Entry.bExists = std::filesystem::exists(FilePath, Ec) && !Ec;
    if (Entry.bExists)
    {
        Entry.LastWriteTime = std::filesystem::last_write_time(FilePath, Ec);
        if (Ec)
        {
            Entry.bExists = false;
            Entry.LastWriteTime = {};
        }
    }

    return Entry;
}

bool UTexture2D::HasCacheEntryChanged(const FTextureCacheEntry& Entry)
{
    if (Entry.FullPath.empty())
    {
        return false;
    }

    const FTextureCacheEntry Current = BuildCacheEntry(ResolveFullPath(Entry.FullPath));
    if (Current.bExists != Entry.bExists)
    {
        return true;
    }

    if (!Current.bExists)
    {
        return false;
    }

    return Current.LastWriteTime != Entry.LastWriteTime;
}
