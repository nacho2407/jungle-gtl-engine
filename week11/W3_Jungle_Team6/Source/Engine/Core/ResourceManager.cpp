#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <fstream>
#include <filesystem>
#include "DDSTextureLoader.h"
#include <UI/EditorConsoleWidget.h>

namespace ResourceKey
{
	constexpr const char* Font = "Font";
	constexpr const char* Particle = "Particle";
}

void FResourceManager::LoadFromFile(const FString& Path, ID3D11Device* InDevice)
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
			Resource.ResourceType = EResourceType::Font;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Pair.second.ToString();
			Resource.SRV = nullptr;
			Resources[Pair.first] = Resource;
		}
	}

	if (Root.hasKey(ResourceKey::Particle))
	{
		JSON ParticleSection = Root[ResourceKey::Particle];
		for (auto& Pair : ParticleSection.ObjectRange())
		{
			FFontResource Resource;
			Resource.ResourceType = EResourceType::Particle;
			Resource.Name = FName(Pair.first.c_str());
			Resource.Path = Pair.second.ToString();
			Resource.SRV = nullptr;
			Resources[Pair.first] = Resource;
		}
	}
	if (LoadGPUResources(InDevice))
	{
		UE_LOG("Complete Load Resources!");
	}
	else
	{
		UE_LOG("Failed to Load Resources...");
	}
}

bool FResourceManager::LoadGPUResources(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	// TODO: 파트 C에서 구현
	// 각 FFontResource의 Path를 읽어 텍스처 로드 후 SRV 생성
	for (auto& [Key, Resource] : Resources)
	{
		std::wstring FullPath = FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Resource.Path));
		HRESULT hr = DirectX::CreateDDSTextureFromFileEx(
			Device,
			FullPath.c_str(),
			0,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			DirectX::DDS_LOADER_DEFAULT,
			nullptr,
			&Resource.SRV
		);

		if (FAILED(hr)) 
			return false;
	}
	return true;
}

void FResourceManager::ReleaseGPUResources()
{
	for (auto& [Key, Resource] : Resources)
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
	auto It = Resources.find(FontName.ToString());
	if (It != Resources.end())
	{
		return &It->second;
	}
	return nullptr;
}

const FFontResource* FResourceManager::FindFont(const FName& FontName) const
{
	auto It = Resources.find(FontName.ToString());
	if (It != Resources.end())
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
	Resources[FontName.ToString()] = Resource;
}
