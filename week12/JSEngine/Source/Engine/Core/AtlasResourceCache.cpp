#include "Core/AtlasResourceCache.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace
{
	FString NormalizeLookupKey(const FName& Name)
	{
		return FPaths::Normalize(Name.ToString());
	}

	FString GetStem(const FString& Path)
	{
		const std::filesystem::path FsPath(FPaths::ToWide(Path));
		return FPaths::ToUtf8(FsPath.stem().wstring());
	}

	template <typename ResourceType>
	ResourceType* FindAtlasResource(TMap<FString, ResourceType>& Resources, const FName& Name)
	{
		if (Resources.empty())
		{
			return nullptr;
		}

		const FString Key = NormalizeLookupKey(Name);
		auto It = Resources.find(Key);
		if (It != Resources.end())
		{
			return &It->second;
		}

		const FString Stem = GetStem(Key);
		for (auto& [ResourceKey, Resource] : Resources)
		{
			if (GetStem(ResourceKey) == Stem || GetStem(Resource.Path) == Stem)
			{
				return &Resource;
			}
		}

		return &Resources.begin()->second;
	}

	template <typename ResourceType>
	const ResourceType* FindAtlasResource(const TMap<FString, ResourceType>& Resources, const FName& Name)
	{
		if (Resources.empty())
		{
			return nullptr;
		}

		const FString Key = NormalizeLookupKey(Name);
		auto It = Resources.find(Key);
		if (It != Resources.end())
		{
			return &It->second;
		}

		const FString Stem = GetStem(Key);
		for (const auto& [ResourceKey, Resource] : Resources)
		{
			if (GetStem(ResourceKey) == Stem || GetStem(Resource.Path) == Stem)
			{
				return &Resource;
			}
		}

		return &Resources.begin()->second;
	}
}

bool FAtlasResourceCache::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.Texture != nullptr && Resource.Texture->GetSRV() != nullptr)
		{
			continue;
		}

		if (!FontLoader.Load(Resource.Name, Resource.Path, Resource.Columns, Resource.Rows, Device, Resource))
		{
			UE_LOG_WARNING("Failed to load Font atlas: %s", Resource.Path.c_str());
			return false;
		}
	}

	for (auto& [Key, Resource] : SubUVResources)
	{
		if (Resource.Texture != nullptr && Resource.Texture->GetSRV() != nullptr)
		{
			continue;
		}

		if (!SubUVLoader.Load(Resource.Name, Resource.Path, Resource.Columns, Resource.Rows, Device, Resource))
		{
			UE_LOG_WARNING("Failed to load SubUV atlas: %s", Resource.Path.c_str());
			return false;
		}
	}

	return true;
}

FFontResource* FAtlasResourceCache::FindFont(const FName& FontName)
{
	return FindAtlasResource(FontResources, FontName);
}

const FFontResource* FAtlasResourceCache::FindFont(const FName& FontName) const
{
	return FindAtlasResource(FontResources, FontName);
}

void FAtlasResourceCache::RegisterFont(const FName& FontName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FFontResource Resource;
	Resource.Name = FontName;
	Resource.Path = FPaths::Normalize(InPath);
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.Texture = UObjectManager::Get().CreateObject<UTexture>();
	FontResources[FPaths::Normalize(FontName.ToString())] = Resource;
}

FSubUVResource* FAtlasResourceCache::FindSubUV(const FName& SubUVName)
{
	return FindAtlasResource(SubUVResources, SubUVName);
}

const FSubUVResource* FAtlasResourceCache::FindSubUV(const FName& SubUVName) const
{
	return FindAtlasResource(SubUVResources, SubUVName);
}

void FAtlasResourceCache::RegisterSubUV(const FName& SubUVName, const FString& InPath, uint32 Columns, uint32 Rows)
{
	FSubUVResource Resource;
	Resource.Name = SubUVName;
	Resource.Path = FPaths::Normalize(InPath);
	Resource.Columns = Columns;
	Resource.Rows = Rows;
	Resource.Texture = UObjectManager::Get().CreateObject<UTexture>();
	SubUVResources[FPaths::Normalize(SubUVName.ToString())] = Resource;
}

void FAtlasResourceCache::Clear()
{
	Release();
}

void FAtlasResourceCache::Release()
{
	std::unordered_set<UObject*> DestroyedObjects;
	auto DestroyUniqueObject = [&DestroyedObjects](UObject* Object)
	{
		if (Object && DestroyedObjects.insert(Object).second)
		{
			UObjectManager::Get().DestroyObject(Object);
		}
	};

	for (auto& [Key, Font] : FontResources)
	{
		DestroyUniqueObject(Font.Texture);
	}
	FontResources.clear();

	for (auto& [Key, SubUV] : SubUVResources)
	{
		DestroyUniqueObject(SubUV.Texture);
	}
	SubUVResources.clear();
}
