#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>

namespace ResourceKey
{
	constexpr const char* Font = "Font";
}

void FResourceManager::LoadFromFile(const FString& Path)
{
	using namespace json;

	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON Root = JSON::Load(Content);

	// Font
	if (Root.hasKey(ResourceKey::Font))
	{
		JSON FontSection = Root[ResourceKey::Font];
		for (auto& Pair : FontSection.ObjectRange())
		{
			FFontResource Resource;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Pair.second.ToString();
			Resource.SRV = nullptr;
			FontResources[Pair.first] = Resource;
		}
	}
}

void FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return;
	}

	// TODO: 파트 C에서 구현
	// 각 FFontResource의 Path를 읽어 텍스처 로드 후 SRV 생성
	// for (auto& [Key, Resource] : FontResources)
	// {
	//     std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));
	//     Resource.SRV = LoadTextureFromFile(Device, FullPath);
	// }
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : FontResources)
	{
		if (Resource.SRV)
		{
			Resource.SRV->Release();
			Resource.SRV = nullptr;
		}
	}
}

FFontResource* FResourceManager::FindFont(const FName& FontName)
{
	auto It = FontResources.find(FontName.ToString());
	if (It != FontResources.end())
	{
		return &It->second;
	}
	return nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	auto It = FontResources.find(FontName.ToString());
	if (It != FontResources.end())
	{
		return &It->second;
	}
	return nullptr;
}

void FResourceManager::RegisterFont(const FName& FontName, const FString& InPath)
{
	FFontResource Resource;
	Resource.Name = FontName;
	Resource.Path = InPath;
	Resource.SRV = nullptr;
	FontResources[FontName.ToString()] = Resource;
}
