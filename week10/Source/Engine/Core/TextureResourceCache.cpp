#include "Core/TextureResourceCache.h"

#include "Core/Paths.h"
#include "Object/ObjectFactory.h"

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

ID3D11ShaderResourceView* FTextureResourceCache::GetDefaultWhiteSRV() const
{
	UTexture* Texture = Get("DefaultWhite");
	return Texture ? Texture->GetSRV() : nullptr;
}

UTexture* FTextureResourceCache::Get(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = Textures.find(NormalizedPath);
	return (It != Textures.end()) ? It->second : nullptr;
}

UTexture* FTextureResourceCache::Load(const FString& Path, ID3D11Device* Device)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || Device == nullptr)
	{
		return nullptr;
	}

	const uint64 CurrentWriteTimeTicks = GetFileWriteTimeTicks(NormalizedPath);
	if (UTexture* Cached = Get(NormalizedPath))
	{
		const auto WriteTimeIt = TextureWriteTimeTicks.find(NormalizedPath);
		const uint64 CachedWriteTimeTicks =
			(WriteTimeIt != TextureWriteTimeTicks.end()) ? WriteTimeIt->second : 0;
		if (CurrentWriteTimeTicks == 0 || CachedWriteTimeTicks == CurrentWriteTimeTicks)
		{
			return Cached;
		}

		if (!Cached->LoadFromFile(NormalizedPath, Device))
		{
			return Cached;
		}

		TextureWriteTimeTicks[NormalizedPath] = CurrentWriteTimeTicks;
		return Cached;
	}

	UTexture* Texture = UObjectManager::Get().CreateObject<UTexture>();
	if (!Texture->LoadFromFile(NormalizedPath, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	Textures[NormalizedPath] = Texture;
	TextureWriteTimeTicks[NormalizedPath] = CurrentWriteTimeTicks;
	return Texture;
}

void FTextureResourceCache::Register(const FString& Key, UTexture* Texture, uint64 WriteTimeTicks)
{
	const FString NormalizedKey = FPaths::Normalize(Key);
	if (NormalizedKey.empty() || Texture == nullptr)
	{
		return;
	}

	Textures[NormalizedKey] = Texture;
	TextureWriteTimeTicks[NormalizedKey] = WriteTimeTicks;
}

bool FTextureResourceCache::Contains(const FString& Key) const
{
	return Get(Key) != nullptr;
}

void FTextureResourceCache::Release()
{
	for (auto& [Key, Texture] : Textures)
	{
		if (Texture)
		{
			UObjectManager::Get().DestroyObject(Texture);
		}
	}
	Textures.clear();
	TextureWriteTimeTicks.clear();
}

uint64 FTextureResourceCache::GetFileWriteTimeTicks(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	fs::path FilePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedPath)));
	if (!fs::exists(FilePath))
	{
		return 0;
	}

	const auto WriteTime = fs::last_write_time(FilePath);
	return static_cast<uint64>(WriteTime.time_since_epoch().count());
}
